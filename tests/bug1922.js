import { assert } from "./assert.js";

function g(a, d = (a = 10)) { return a; }
function k(a, c = () => a, d = (a = 10)) { return [a, c()]; }

function f(a, set = () => { a = 5; }) { set(); return a; }

assert(g(1), 10);
assert(JSON.stringify(k(1)), JSON.stringify([10, 10]));
assert(f(1), 5);

function f1(a, c = () => a) { var a; a = 2; return [a, c()] }
assert(f1(1), [2, 1]);

function f2(a, c = () => a) { var a = 3; return [a, c()] }
assert(f2(1), [3, 1]);

function f3(a = 1, c = () => { a = 9 }) { var a; c(); return a }
assert(f3(), 1);

function f4(a, b = () => a) { function a() {} return typeof a }
assert(f4(1), "function");

function f5(a, b = () => a) { eval("a = 7"); return a }
assert(f5(1), 7);

const f6 = Function("a", "b = () => a", 'eval("var a = 7"); return a;');
assert(f6(1), 7);

function f7(a, b = (a = 10)) {
    var a;
    return a;
}
assert(f7(1), 10);

function f8(a, set = () => { a = 5; }) {
    eval("");
    set();
    return a;
}
assert(f8(1), 5);