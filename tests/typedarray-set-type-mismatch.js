import { assertThrows } from "./assert.js";

assertThrows(TypeError, () => new BigInt64Array(0).set(new Int8Array(0)));
assertThrows(TypeError, () => new Int8Array(0).set(new BigInt64Array(0)));

assertThrows(TypeError, () => new BigUint64Array(0).set(new Int8Array(0)));
assertThrows(TypeError, () => new Int8Array(0).set(new BigUint64Array(0)));

assertThrows(TypeError, () => new BigInt64Array(0).set(new Uint8Array(0)));
assertThrows(TypeError, () => new Uint8Array(0).set(new BigInt64Array(0)));

assertThrows(TypeError, () => new BigUint64Array(0).set(new Uint8Array(0)));
assertThrows(TypeError, () => new Uint8Array(0).set(new BigUint64Array(0)));