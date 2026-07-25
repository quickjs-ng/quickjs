import { assert } from "./assert.js";

/* %TypedArray%.prototype.slice copies byte by byte in increasing order
   (step 14.g). This is observable when the species constructor returns a
   view over the same buffer that overlaps the source ahead of it: already
   copied elements are read back as source values. */

function check(TA) {
    const ta = new TA([10, 20, 30, 40, 50, 60]);
    ta.constructor = {
        [Symbol.species]: function() {
            return new TA(ta.buffer, 2 * TA.BYTES_PER_ELEMENT);
        }
    };
    const result = ta.slice(1, 4);
    assert(result.length, 4);
    assert(result[0], 20);
    assert(result[1], 20);
    assert(result[2], 20);
    assert(result[3], 60);
}

check(Float64Array);
check(Int32Array);
check(Uint8Array);

/* target view behind the source: plain move semantics are unaffected */
{
    const ta = new Int32Array([10, 20, 30, 40, 50, 60]);
    ta.constructor = {
        [Symbol.species]: function() {
            return new Int32Array(ta.buffer, 0);
        }
    };
    const result = ta.slice(2, 5);
    assert(result[0], 30);
    assert(result[1], 40);
    assert(result[2], 50);
}

/* distinct buffers keep working */
{
    const ta = new Int32Array([1, 2, 3, 4]);
    const result = ta.slice(1, 3);
    assert(result.length, 2);
    assert(result[0], 2);
    assert(result[1], 3);
}
