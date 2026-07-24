import { assert } from "./assert.js";

/* TypedArray [[Set]] (10.4.5.5): an out-of-bounds or non-canonical integer
   index only coerces the value when the receiver is the typed array itself.
   With any other receiver the invalid index is dropped (step ii) without
   evaluating the value. */

/* receiver is a primitive: value must not be coerced */
{
    let coerced = 0;
    const value = { valueOf() { coerced++; return 1; } };
    const ta = new Int32Array(10);
    assert(Reflect.set(ta, 100, value, "not an object"), true);
    assert(coerced, 0);
}

/* receiver is a different object: value must not be coerced */
{
    let coerced = 0;
    const value = { valueOf() { coerced++; return 1; } };
    const ta = new Int32Array(10);
    assert(Reflect.set(ta, 100, value, {}), true);
    assert(coerced, 0);
}

/* BigInt typed array, different receiver: value must not be coerced */
{
    let coerced = 0;
    const value = { valueOf() { coerced++; return 1n; } };
    const ta = new BigInt64Array(10);
    assert(Reflect.set(ta, 100, value, {}), true);
    assert(coerced, 0);
}

/* receiver is the typed array itself: value IS coerced (TypedArraySetElement),
   even for an out-of-bounds index */
{
    let coerced = 0;
    const value = { valueOf() { coerced++; return 1; } };
    const ta = new Int32Array(10);
    assert(Reflect.set(ta, 100, value, ta), true);
    assert(coerced, 1);
}

/* an in-bounds index with the typed array as receiver still stores */
{
    const ta = new Int32Array(4);
    assert(Reflect.set(ta, 2, 42, ta), true);
    assert(ta[2], 42);
}
