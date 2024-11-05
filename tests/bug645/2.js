import { assert, assertThrows } from "../assert.js";
const ab = new ArrayBuffer(16, { maxByteLength: 32 });
const u8 = new Uint8Array(ab, 16);
ab.resize(8);
assert(ab.byteLength, 8);
u8[1] = 123; // Doesn't throw.
assertThrows(TypeError, () => Object.defineProperty(u8, "1", { value: 123 }));
