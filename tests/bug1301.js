/*---
features: [skip-if-tcc]
---*/

import {assert} from "./assert.js"

const rab = new ArrayBuffer(1024, { maxByteLength: 1024 * 1024 });
const i32 = new Int32Array(rab);
const evil = {
    valueOf: () => {
        print("resize buffer");
        rab.resize(0);
        return 123;
    }
};

try {
  Atomics.store(i32, 0, evil);
} catch (e) {
  assert(e instanceof RangeError);
}
