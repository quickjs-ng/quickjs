import { assert } from "./assert.js";

function g(a, d = (a = 10)) { return a; }
function k(a, c = () => a, d = (a = 10)) { return [a, c()]; }

function f(a, set = () => { a = 5; }) { set(); return a; }

assert(g(1), 10);
assert(JSON.stringify(k(1)), JSON.stringify([10, 10]));
assert(f(1), 5);