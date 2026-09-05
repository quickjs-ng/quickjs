import { assertThrows } from "./assert.js";

assertThrows(TypeError, function () {new Int8Array(new BigInt64Array());});
assertThrows(TypeError, function () {new Int8Array(new BigUint64Array());});
assertThrows(TypeError, function () {new BigInt64Array(new Int8Array());});
assertThrows(TypeError, function () {new BigUint64Array(new Int8Array());});