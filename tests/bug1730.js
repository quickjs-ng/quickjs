import { assert } from "./assert.js";

assert(Reflect.set({ set x(v) {} }, "x", 1, null), true);
assert(Reflect.set({}, "x", 1, undefined), false);
assert(Reflect.set({}, "x", 1, null), false);