import { assert } from "./assert.js";

// https://github.com/quickjs-ng/quickjs/issues/833
// In a classic (non-generator) function `yield` is a plain identifier. When it
// is followed by a value, the user almost certainly meant the generator
// keyword, so the parser now emits a clearer error than the generic
// "expecting ';'".

function compileError(src) {
    try {
        // eslint-disable-next-line no-eval
        (0, eval)(src);
    } catch (e) {
        return e;
    }
    return null;
}

// `yield <value>` in a non-generator function reports the improved message.
for (const src of [
    "(function g(){ yield 42; })",
    "(async function g(){ yield 42; })",
    "(function g(){ yield foo; })",
    "(function g(){ yield \"x\"; })",
    "(function g(){ yield true; })",
]) {
    const e = compileError(src);
    assert(e instanceof SyntaxError, true, src);
    assert(e.message.includes("generator"), true,
           "expected clearer message for: " + src + " (got: " + e.message + ")");
    assert(e.message.includes("expecting ';'"), false,
           "should no longer emit generic message for: " + src);
}

// Negative: `yield` used as a plain identifier in sloppy mode still parses.
for (const src of [
    "var yield = 1; yield;",
    "var yield = 1; yield + 1;",
    "var yield = function(){ return 1; }; yield();",
    "var yield = [1]; yield[0];",
    "var yield = { a: 1 }; yield.a;",
    "var yield; yield = 5;",
    "var yield = 1; yield++;",
]) {
    assert(compileError(src), null, "yield-as-identifier should parse: " + src);
}

// Sanity: a real generator still compiles and runs.
assert(compileError("(function* g(){ yield 42; })"), null,
       "generator function should still compile");
function* g() { yield 42; }
assert(g().next().value, 42, "generator still yields");
