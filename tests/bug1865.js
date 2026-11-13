// https://github.com/quickjs-ng/quickjs/issues/1865
// A function capturing more than 65535 variables wrapped var_ref_count and
// var_ref_idx to 16 bits and under-allocated the frame's var_refs array.
import { assert, assertThrows } from "./assert.js";

function names(prefix, n) {
    const a = [];
    for (let i = 0; i < n; i++)
        a.push(prefix + i);
    return a;
}

const params = names("a", 32768);

function makeSource(nvars) {
    const vars = names("v", nvars);
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

// A direct eval captures every argument and variable of the caller.
assertThrows(SyntaxError, () => (0, eval)(`(function (${params}) {
    var ${names("v", 32768)};
    eval("");
})`));

// A mapped arguments object captures every argument.
function makeArgumentsSource(nvars) {
    const vars = names("v", nvars);
    return `(function* (${params}) {
        var ${vars};
        (() => [${vars}]);
        yield arguments;
    })`;
}

assertThrows(SyntaxError, () => (0, eval)(makeArgumentsSource(32768)));

const args = (0, eval)(makeArgumentsSource(32767))(1, 2).next().value;
assert(args.length, 2);
assert(args[1], 2);
