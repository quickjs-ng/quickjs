import * as std from "qjs:std";
import { assert } from "./assert.js";

// Regression test: a suspended coroutine reachable only through a closure that
// captured one of its locals must not be collected while it is still live.
//
// A closure capturing a coroutine local holds an *open* var_ref into the
// coroutine's frame. Open var_refs are not GC objects and are not marked by
// the closure, so the edge closure -> open var_ref -> value is invisible to
// the cycle collector. An ordinary function's frame is a live C-stack root, but
// a suspended coroutine's frame is a heap GC object, so if the only thing
// keeping it reachable is such an escaped closure, the whole still-live
// coroutine used to be collected -- and resuming it, or running the closure,
// then touched freed memory.
//
// The async function below suspends at `await d.promise`; that promise is never
// resolved, so nothing keeps the coroutine reachable except `leaked`'s capture
// of the frame local `d`. Forcing a GC while it is suspended used to free it.

globalThis.leaked = null;

(function () {
    async function step() {
        const d = Promise.withResolvers();
        globalThis.leaked = () => d; // escapes, capturing the coroutine local `d`
        await d.promise;             // suspend: reachable only via leaked -> d
    }
    step();
})();

std.gc();

// If the suspended coroutine's frame was wrongly collected, `d` is freed memory.
const d = leaked();
assert(typeof d.resolve, "function");
