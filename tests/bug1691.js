import { assertThrows } from "./assert.js";

assertThrows(TypeError, function () {Object.preventExtensions(new Int8Array(new ArrayBuffer(16, { maxByteLength: 16 })));});