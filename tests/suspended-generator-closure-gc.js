import * as std from "qjs:std";
import { assert } from "./assert.js";

// Regression test: a suspended generator kept reachable only through a closure
// that captured one of its locals must not be collected.
//
// This is the sync-generator counterpart of suspended-coroutine-closure-gc.js:
// there is no promise involved. The generator object is held in a cycle through
// a captured local, and the escaped closure is the only external anchor. The
// closure holds an open var_ref into the generator's (heap) frame; if that edge
// is invisible to the cycle collector the generator and its frame are freed
// while the closure still points into them.

globalThis.leaked = null;

(function () {
    let g;
    function* gen() {
        const o = {};
        o.g = g;                       // frame local -> generator object (a cycle)
        globalThis.leaked = () => o;   // escapes, capturing the frame local `o`
        yield;                         // suspend
    }
    g = gen();
    g.next();                          // run to the yield
    g = null;                          // now reachable only via the cycle + leaked
})();

std.gc();

const o = leaked();
assert(o.g !== null && o.g !== undefined);   // the generator object is still alive
o.g.next();                                   // resume it: touches its frame
