// https://github.com/quickjs-ng/quickjs/issues/1760
import { assertThrows } from "./assert.js";

// The maxByteLength option is converted with ToIndex, which throws a
// RangeError for +Infinity and for anything above 2^53-1.
assertThrows(RangeError, () => new ArrayBuffer(0, { maxByteLength: Infinity }));
assertThrows(RangeError, () => new ArrayBuffer(0, { maxByteLength: 2 ** 64 }));
assertThrows(RangeError, () => new SharedArrayBuffer(0, { maxByteLength: Infinity }));

// A negative maxByteLength is rejected before the prototype is read from
// newTarget.
const newTarget = new Proxy(function () {}, { get() { throw new Error("prototype read"); } });
assertThrows(RangeError, () => Reflect.construct(ArrayBuffer, [0, { maxByteLength: -1 }], newTarget));
