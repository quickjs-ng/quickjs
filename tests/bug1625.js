import { assertThrows } from "./assert.js";


assertThrows(TypeError, () => new BigInt64Array().with());
assertThrows(TypeError, () => new BigInt64Array().with(0, 1));
assertThrows(RangeError, () => new BigInt64Array().with(0, BigInt(10)));