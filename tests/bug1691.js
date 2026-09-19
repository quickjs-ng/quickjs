import { assertThrows } from "./assert.js";

const ab = new ArrayBuffer(16, { maxByteLength: 16 });
const ta = new Int8Array(ab);
assertThrows(TypeError, function() { Object.preventExtensions(ta) });