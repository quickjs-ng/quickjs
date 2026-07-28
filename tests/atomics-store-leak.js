/*---
features: [skip-if-tcc]
---*/

import { assert } from "./assert.js";

const big = 0x123456789abcn;

// Detached buffer.
{
    const ab = new ArrayBuffer(8);
    const ta = new BigInt64Array(ab);
    const evil = { valueOf() { ab.transfer(); return big; } };
    let err;
    try {
        Atomics.store(ta, 0, evil);
    } catch (e) {
        err = e;
    }
    assert(err instanceof RangeError, true);
}

// Shrunk resizable buffer.
{
    const ab = new ArrayBuffer(8, { maxByteLength: 16 });
    const ta = new BigUint64Array(ab);
    const evil = { valueOf() { ab.resize(0); return big; } };
    let err;
    try {
        Atomics.store(ta, 0, evil);
    } catch (e) {
        err = e;
    }
    assert(err instanceof RangeError, true);
}

