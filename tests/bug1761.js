// https://github.com/quickjs-ng/quickjs/issues/1761
import { assert, assertThrows } from "./assert.js";

// resize and grow convert their argument with ToIndex, which throws a
// RangeError for +Infinity and for anything above 2^53-1.
const ab = new ArrayBuffer(8, { maxByteLength: 8192 });
assertThrows(RangeError, () => ab.resize(Infinity));
assertThrows(RangeError, () => ab.resize(2 ** 64 + 4096));
assert(ab.byteLength, 8);

const sab = new SharedArrayBuffer(8, { maxByteLength: 8192 });
assertThrows(RangeError, () => sab.grow(Infinity));
assertThrows(RangeError, () => sab.grow(2 ** 64 + 4096));
assert(sab.byteLength, 8);

// A buffer that is not resizable throws a TypeError before the argument
// is converted.
const len = { valueOf() { throw new Error("valueOf called"); } };
assertThrows(TypeError, () => new ArrayBuffer(8).resize(len));
assertThrows(TypeError, () => new SharedArrayBuffer(8).grow(len));
