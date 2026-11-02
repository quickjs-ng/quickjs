// https://github.com/quickjs-ng/quickjs/issues/1865
// A function capturing more than 65535 variables wrapped var_ref_count and
// var_ref_idx to 16 bits and under-allocated the frame's var_refs array.
import { assert, assertThrows } from "./assert.js";

function makeSource(nvars) {
    const params = [], vars = [];
    for (let i = 0; i < 32768; i++)
        params.push("a" + i);
    for (let i = 0; i < nvars; i++)
        vars.push("v" + i);
    // Split the captures over two arrows: a single arrow can only hold 65535
    // closure variables. Neither arrow is ever called.
    return `(function* (${params}) {
        var ${vars};
        (() => [${params}]);
        (() => [${vars}]);
        v0 = 1;
        yield a0 + v0;
    })`;
}

// 32768 arguments and 32768 variables is one captured variable too many.
assertThrows(SyntaxError, () => (0, eval)(makeSource(32768)));

// 32768 arguments and 32767 variables is the largest count that still fits.
const gen = (0, eval)(makeSource(32767));
const res = gen(41).next();
assert(res.done, false);
assert(res.value, 42);
