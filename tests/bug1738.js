// https://github.com/quickjs-ng/quickjs/issues/1738
import { assert, assertThrows } from "./assert.js";

function thrown(f) {
    try {
        f();
    } catch (e) {
        return e;
    }
    return undefined;
}

// A misaligned offset is a RangeError, even when the buffer is detached.
var b = new ArrayBuffer(8);
b.transfer();
assertThrows(RangeError, () => new Uint16Array(b, 1));

// The length is coerced before the buffer is checked for detachment.
b = new ArrayBuffer(8);
b.transfer();
assert(thrown(() => new Uint16Array(b, 4, { valueOf() { throw 0 } })), 0);

// The length is coerced before the offset is checked against the buffer.
assert(thrown(() => new Uint16Array(new ArrayBuffer(2), 8, { valueOf() { throw 0 } })), 0);
assert(thrown(() => new Float16Array(new ArrayBuffer(1), 4, { valueOf() { throw 0 } })), 0);

// Coercing the length may detach the buffer.
b = new ArrayBuffer(8);
assertThrows(TypeError, () => new Uint8Array(b, 0, { valueOf() { b.transfer(); return 1 } }));

// The checks themselves are unchanged.
assertThrows(RangeError, () => new Uint16Array(new ArrayBuffer(8), 1));
assertThrows(RangeError, () => new Uint16Array(new ArrayBuffer(8), 10));
assertThrows(RangeError, () => new Uint16Array(new ArrayBuffer(8), 4, 4));
assertThrows(RangeError, () => new Uint16Array(new ArrayBuffer(7)));
b = new ArrayBuffer(8);
b.transfer();
assertThrows(TypeError, () => new Uint16Array(b, 4));
assert(thrown(() => new Uint16Array(new ArrayBuffer(8), 4, { valueOf() { throw 0 } })), 0);
var a = new Uint16Array(new ArrayBuffer(8), 2, 2);
assert(a.length, 2);
assert(a.byteOffset, 2);
a = new Uint16Array(new ArrayBuffer(8), 4);
assert(a.length, 2);
