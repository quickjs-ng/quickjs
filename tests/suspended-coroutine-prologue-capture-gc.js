import * as std from "qjs:std";
import { assert } from "./assert.js";

// Regression test (quickjs-ng issue #1910): a generator or async generator
// kept reachable only through a closure (or `arguments`) created in its
// *prologue* must not be collected while it is still live.
//
// A generator runs its prologue -- default parameter expressions, building the
// `arguments` object, destructuring -- during the initial resume, which the
// engine performs before the generator object itself exists. The fix for
// suspended-coroutine closures (#1569) makes a var_ref that captures a frame
// local hold a counted reference to the owning coroutine, but for generators
// that owner is only known *after* the initial resume. A var_ref the prologue
// creates was therefore left "non-coro": it kept neither a reference nor a GC
// edge to the generator, so a closure escaping it could outlive the generator,
// and the cycle collector would free the frame it still pointed into
// (use-after-free, crashing in gc_decref_child under a churned/instrumented
// heap). The prologue-created var_refs are now adopted once the owner exists.

// Generator: a closure created in a default parameter captures `arguments`.
// After the yields, arguments[0] holds the generator itself, so the only live
// path to it is leaked -> arguments -> var_ref -> frame.
globalThis.leaked = null;
(function () {
    function* g(a = (globalThis.leaked = () => arguments)) {
        arguments[0] = yield 1;
        yield 2;
    }
    const it = g();
    assert(it.next().value, 1);
    assert(it.next(it).value, 2);  // arguments[0] = it
})();

std.gc();

// If the generator was wrongly collected, this reads freed memory.
const args = leaked();
assert(typeof args[0], "object");
assert(args[0].next().done, true);  // the generator is still live and resumes

// Async generator: same prologue capture, driven to the second yield through
// the microtask queue with top-level await.
globalThis.leakedAsync = null;
await (async function () {
    async function* g(a = (globalThis.leakedAsync = () => arguments)) {
        arguments[0] = yield 1;
        yield 2;
    }
    const it = g();
    await it.next();
    await it.next(it);  // arguments[0] = it
})();

std.gc();

const asyncArgs = leakedAsync();
assert(typeof asyncArgs[0], "object");
