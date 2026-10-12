// https://github.com/quickjs-ng/quickjs/issues/1765
import { assert, assertThrows } from "./assert.js";

// A detached fixed-length view is still fixed-length: freeze succeeds.
let v = new Float32Array(new ArrayBuffer(8));
v.buffer.transfer();
assert(Object.freeze(v) === v, true);
assert(Object.isFrozen(v), true);
assert(Object.isExtensible(v), false);
assert(Object.keys(v).length, 0);
assert(v.length, 0);

v = new Uint8Array(new ArrayBuffer(4));
v.buffer.transfer();
assert(Object.freeze(v) === v, true);
assert(Object.isFrozen(v), true);
assert(Object.isExtensible(v), false);
assert(Object.keys(v).length, 0);
assert(v.length, 0);

// Named own properties are frozen; integer indices stay absent.
v = new Uint8Array(new ArrayBuffer(4));
v.foo = 1;
v.buffer.transfer();
assert(Object.freeze(v) === v, true);
let d = Object.getOwnPropertyDescriptor(v, "foo");
assert(d.value, 1);
assert(d.writable, false);
assert(d.configurable, false);
assert(Object.keys(v).join(","), "foo");
assert(v.length, 0);

// An in-bounds index cannot be made non-writable.
let msg;
try {
    Object.freeze(new Uint8Array(new ArrayBuffer(4)));
} catch (e) {
    assert(e instanceof TypeError, true);
    msg = e.message;
}
assert(msg, "invalid descriptor flags");

// Length-tracking view of a resizable ArrayBuffer.
assertThrows(TypeError, function() {
    Object.freeze(new Uint8Array(new ArrayBuffer(4, { maxByteLength: 8 })));
});

// Fixed-length view of a resizable SharedArrayBuffer: PreventExtensions
// allows it, then the remaining resizable-buffer guard rejects it.
let sab = new SharedArrayBuffer(4, { maxByteLength: 4 });
msg = undefined;
try {
    Object.freeze(new Uint8Array(sab, 0, 4));
} catch (e) {
    assert(e instanceof TypeError, true);
    msg = e.message;
}
assert(msg, "cannot freeze resizable typed array");

v = new Uint8Array(new ArrayBuffer(4));
v.buffer.transfer();
assert(Object.seal(v) === v, true);

let o = { a: 1 };
assert(Object.freeze(o) === o, true);
assert(Object.isFrozen(o), true);
d = Object.getOwnPropertyDescriptor(o, "a");
assert(d.writable, false);
assert(d.configurable, false);
