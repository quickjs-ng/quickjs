import * as std from "qjs:std";
import { assert } from "./assert.js";

// Regression test: a suspended coroutine kept reachable only through its mapped
// `arguments` object must not be collected.
//
// A sloppy function with a simple parameter list that uses `arguments` gets a
// *mapped* arguments object whose entries are open var_refs pointing into the
// function's frame. That object is a third var_ref holder (besides closures and
// object var_ref properties) and must be traced the same way; otherwise a
// suspended coroutine reachable only via an escaped `arguments` is either freed
// while still live (use-after-free) or wrongly retained (a leak that trips the
// gc_obj_list check at runtime teardown).
//
// The coroutine is built with the AsyncFunction constructor so it is sloppy;
// test files are modules (strict), which would otherwise give an unmapped
// arguments object and not exercise this path.

globalThis.leaked = null;

const AsyncFunction = (async function () {}).constructor;
const step = AsyncFunction("a", `
    globalThis.leaked = () => arguments;   // escapes, capturing the frame
    const d = Promise.withResolvers();
    await d.promise;                       // suspend; never resolved
`);
step(123);

std.gc();

// If the frame was wrongly collected this reads freed memory.
assert(leaked()[0], 123);
