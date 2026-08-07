import { assert } from "./assert.js";

/* %TypedArray%.prototype.indexOf: the length is read before the fromIndex
   argument is coerced. When the coercion shrinks a resizable buffer the
   upward scan continues in the still valid prefix instead of returning -1;
   the indices that vanished simply cannot match. */

{
    const cases = [
        /* [fromIndex, searchElement, expected] */
        [0, 2, 2],              /* still inside the surviving prefix */
        [0, 6, -1],             /* vanished with the truncated tail */
        [3, 3, 3],              /* fromIndex inside the surviving prefix */
        [3, 1, -1],             /* present, but before fromIndex */
        [5, 1, -1],             /* fromIndex beyond the new length */
        [-8, 1, 1],             /* negative: resolved against the old length */
        [-2, 1, -1],            /* ... which puts it past the new end */
    ];

    for (const [index, search, expected] of cases) {
        const rab = new ArrayBuffer(8, { maxByteLength: 8 });
        const ta = new Int8Array(rab);
        for (let i = 0; i < ta.length; i++)
            ta[i] = i;
        const evil = {
            valueOf() {
                rab.resize(4);
                return index;
            }
        };
        assert(ta.indexOf(search, evil), expected, `indexOf(${search}, ${index})`);
    }
}

/* every element type takes its own scan path; uint8 uses memchr */
{
    const types = [
        Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array,
        Int32Array, Uint32Array, Float16Array, Float32Array, Float64Array,
    ];

    for (const Ctor of types) {
        const bytes = 8 * Ctor.BYTES_PER_ELEMENT;
        const rab = new ArrayBuffer(bytes, { maxByteLength: bytes });
        const ta = new Ctor(rab);
        for (let i = 0; i < ta.length; i++)
            ta[i] = i;
        const evil = {
            valueOf() {
                rab.resize(bytes / 2);
                return 0;
            }
        };
        assert(ta.indexOf(3, evil), 3, Ctor.name);
    }

    for (const Ctor of [BigInt64Array, BigUint64Array]) {
        const rab = new ArrayBuffer(64, { maxByteLength: 64 });
        const ta = new Ctor(rab);
        for (let i = 0; i < ta.length; i++)
            ta[i] = BigInt(i);
        const evil = {
            valueOf() {
                rab.resize(32);
                return 0;
            }
        };
        assert(ta.indexOf(3n, evil), 3, Ctor.name);
        assert(ta.indexOf(5n, evil), -1, Ctor.name);
    }
}

/* a length tracking view at an offset shrinks along with the buffer */
{
    const rab = new ArrayBuffer(8, { maxByteLength: 8 });
    const ta = new Int8Array(rab, 2);
    for (let i = 0; i < ta.length; i++)
        ta[i] = i;
    assert(ta.length, 6);
    const evil = {
        valueOf() {
            rab.resize(4);
            return 0;
        }
    };
    assert(ta.indexOf(1, evil), 1);
    assert(ta.indexOf(3, evil), -1);
}

/* shrink to zero returns -1 */
{
    const rab = new ArrayBuffer(16, { maxByteLength: 16 });
    const ta = new Int32Array(rab);
    ta.fill(9);
    const evil = {
        valueOf() {
            rab.resize(0);
            return 0;
        }
    };
    assert(ta.indexOf(9, evil), -1);
}

/* a fixed length view that goes out of bounds returns -1, no exception */
{
    const rab = new ArrayBuffer(16, { maxByteLength: 16 });
    const ta = new Int32Array(rab, 0, 4);
    ta.fill(5);
    const evil = {
        valueOf() {
            rab.resize(8);
            return 0;
        }
    };
    assert(ta.indexOf(5, evil), -1);
}

/* a detached buffer returns -1, no exception */
{
    const rab = new ArrayBuffer(8, { maxByteLength: 8 });
    const ta = new Int8Array(rab);
    ta.fill(3);
    const evil = {
        valueOf() {
            rab.transfer();
            return 0;
        }
    };
    assert(ta.indexOf(3, evil), -1);
}

/* growing during the coercion does not extend the scan: the elements added
   past the original length are not searched */
{
    const rab = new ArrayBuffer(4, { maxByteLength: 8 });
    const ta = new Int8Array(rab);
    for (let i = 0; i < ta.length; i++)
        ta[i] = i + 1;
    const evil = {
        valueOf() {
            rab.resize(8);
            return 0;
        }
    };
    assert(ta.indexOf(0, evil), -1);
    assert(ta.indexOf(2, evil), 1);
}

/* includes() keeps reading "undefined" for the indices the shrink removed,
   so it still matches undefined over the truncated tail */
{
    for (const size of [4, 0]) {
        const rab = new ArrayBuffer(8, { maxByteLength: 8 });
        const ta = new Int8Array(rab);
        for (let i = 0; i < ta.length; i++)
            ta[i] = i;
        const evil = {
            valueOf() {
                rab.resize(size);
                return 0;
            }
        };
        assert(ta.includes(undefined, evil), true, `includes(undefined) @ ${size}`);
    }

    /* ... but not once the scan starts past the original length */
    const rab = new ArrayBuffer(8, { maxByteLength: 8 });
    const ta = new Int8Array(rab);
    const evil = {
        valueOf() {
            rab.resize(4);
            return 8;
        }
    };
    assert(ta.includes(undefined, evil), false);
}
