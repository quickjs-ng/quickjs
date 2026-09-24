// https://github.com/quickjs-ng/quickjs/issues/1731
import { assert, assertThrows } from "./assert.js";

function setlike(size) {
    return { size, has: () => false, keys: () => [][Symbol.iterator]() };
}

// GetSetRecord applies ToIntegerOrInfinity before the negative size check,
// so a size in (-1, 0) truncates to 0 and is legal.
assert(new Set([1]).union(setlike(-0.5)).size, 1);
assert(new Set([1]).intersection(setlike(-0.9)).size, 0);
assert(new Set([1]).isSubsetOf(setlike(-0)), false);

// The has getter is reached, so its exception propagates.
var thrown = {};
var caught;
try {
    new Set().intersection({ get has() { throw thrown }, size: -0.5 });
} catch (e) {
    caught = e;
}
assert(caught, thrown);

assertThrows(RangeError, () => new Set().union(setlike(-1)));
assertThrows(RangeError, () => new Set().union(setlike(-Infinity)));
assertThrows(TypeError, () => new Set().union(setlike(NaN)));
assertThrows(TypeError, () => new Set().union(setlike(undefined)));
