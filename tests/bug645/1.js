import { assert, assertThrows } from "../assert.js";
const ab = new ArrayBuffer(1);
const u8 = new Uint8Array(ab);
assert(!ab.detached);
// Detach the ArrayBuffer.
ab.transfer();
assert(ab.detached);
u8[100] = 123; // Doesn't throw.
assertThrows(TypeError, () => Object.defineProperty(u8, "100", { value: 123 }));
