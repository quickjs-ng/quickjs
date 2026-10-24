// https://github.com/quickjs-ng/quickjs/issues/1835
// Freeing a WeakMap whose values are symbols that are also its keys crashed
// in the WeakMap finalizer. Run under ASAN.
import { assert } from "./assert.js";

function f() {
    var o = {};
    var m = new WeakMap(), b = Symbol("b");
    m.set(o, b);
    m.set(b, 1);
    assert(m.get(o), b);
    assert(m.get(b), 1);
    b = undefined;
    m = undefined;
}
f();

function g() {
    var o = {};
    var m = new WeakMap(), a = Symbol("a"), b = Symbol("b"), c = Symbol("c");
    m.set(o, a);
    m.set(a, b);
    m.set(b, c);
    m.set(c, 1);
    a = b = c = undefined;
    m = undefined;
}
g();

// freed when the runtime is freed
const m = new WeakMap();
const a = Symbol("a"), b = Symbol("b");
m.set(a, b);
m.set(b, a);
assert(m.get(a), b);
assert(m.get(b), a);
