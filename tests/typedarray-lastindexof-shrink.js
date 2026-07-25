import { assert } from "./assert.js";

/* %TypedArray%.prototype.lastIndexOf: the length is read before the
   fromIndex argument is coerced. When the coercion shrinks a resizable
   buffer the downward scan skips the vanished indices and continues in
   the still valid range instead of returning -1. */

{
    const rab = new ArrayBuffer(32, { maxByteLength: 32 });
    const ta = new Float64Array(rab);

    const cases = [
        [-1, 2],
        [-2, 2],
        [-3, 1],
        [-4, 0],
        [-5, -1],
    ];

    for (const [index, expected] of cases) {
        rab.resize(32);
        ta.fill(123);
        const evil = {
            valueOf() {
                rab.resize(24);
                return index;
            }
        };
        assert(ta.lastIndexOf(123, evil), expected);
    }
}

/* positive fromIndex beyond the shrunk length clamps to the new end */
{
    const rab = new ArrayBuffer(32, { maxByteLength: 32 });
    const ta = new Int32Array(rab);
    ta.fill(7);
    const evil = {
        valueOf() {
            rab.resize(8);
            return 6;
        }
    };
    assert(ta.lastIndexOf(7, evil), 1);
}

/* a fixed-length view that goes out of bounds still returns -1 */
{
    const rab = new ArrayBuffer(16, { maxByteLength: 16 });
    const ta = new Int32Array(rab, 0, 4);
    ta.fill(5);
    const evil = {
        valueOf() {
            rab.resize(8);
            return 3;
        }
    };
    assert(ta.lastIndexOf(5, evil), -1);
}

/* shrink to zero returns -1 */
{
    const rab = new ArrayBuffer(16, { maxByteLength: 16 });
    const ta = new Int32Array(rab);
    ta.fill(9);
    const evil = {
        valueOf() {
            rab.resize(0);
            return 3;
        }
    };
    assert(ta.lastIndexOf(9, evil), -1);
}
