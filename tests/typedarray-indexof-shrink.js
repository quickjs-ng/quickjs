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

/* lastIndexOf scans downward and clamps its start to the surviving prefix,
   so the same shrink lands it in a different place than indexOf */
{
    function shrunk(ret) {
        const rab = new ArrayBuffer(8, { maxByteLength: 8 });
        const ta = new Int8Array(rab);
        for (let i = 0; i < 8; i++)
            ta[i] = i;
        return [ta, { valueOf() { rab.resize(4); return ret; } }];
    }

    /* fromIndex 7 is clamped to the new last index */
    {
        const [ta, evil] = shrunk(7);
        assert(ta.lastIndexOf(3, evil), 3);
    }
    {
        const [ta, evil] = shrunk(7);
        assert(ta.lastIndexOf(6, evil), -1); /* vanished with the tail */
    }
    /* fromIndex 0 only ever looks at index 0 */
    {
        const [ta, evil] = shrunk(0);
        assert(ta.lastIndexOf(0, evil), 0);
    }
    {
        const [ta, evil] = shrunk(0);
        assert(ta.lastIndexOf(2, evil), -1);
    }
    /* a negative fromIndex resolves against the original length */
    {
        const [ta, evil] = shrunk(-6);
        assert(ta.lastIndexOf(2, evil), 2);
    }
    {
        const [ta, evil] = shrunk(-8);
        assert(ta.lastIndexOf(1, evil), -1);
    }
}

/* indexOf uses strict equality and includes uses SameValueZero, which is
   only visible for NaN and -0 */
{
    const t = new Float64Array([NaN, 0, 1]);
    assert(t.indexOf(NaN), -1);
    assert(t.includes(NaN), true);
    assert(t.indexOf(-0), 1);
    assert(t.includes(-0), true);
    assert(t.indexOf(0), 1);

    const f16 = new Float16Array([NaN, 1]);
    assert(f16.indexOf(NaN), -1);
    assert(f16.includes(NaN), true);

    /* an integer array can never hold either, so neither ever matches */
    const i = new Int32Array([0, 1]);
    assert(i.indexOf(NaN), -1);
    assert(i.includes(NaN), false);
    assert(i.indexOf(-0), 0);

    /* the same, with a shrink in between */
    const rab = new ArrayBuffer(32, { maxByteLength: 32 });
    const ta = new Float64Array(rab);
    ta[0] = NaN;
    ta[3] = NaN;
    const evil = { valueOf() { rab.resize(16); return 0; } };
    assert(ta.indexOf(NaN, evil), -1);
}

/* the fromIndex coercion is still an ordinary ToInteger: its failures
   propagate rather than being swallowed like a resize */
{
    const ta = new Int8Array(4);

    let threw = null;
    try {
        ta.indexOf(0, Symbol("s"));
    } catch (e) {
        threw = e;
    }
    assert(threw instanceof TypeError, true);

    threw = null;
    try {
        ta.indexOf(0, { valueOf() { throw new RangeError("boom"); } });
    } catch (e) {
        threw = e;
    }
    assert(threw instanceof RangeError, true);
    assert(threw.message, "boom");

    threw = null;
    try {
        ta.lastIndexOf(0, { valueOf() { throw new RangeError("boom"); } });
    } catch (e) {
        threw = e;
    }
    assert(threw instanceof RangeError, true);

    threw = null;
    try {
        ta.includes(0, { valueOf() { throw new RangeError("boom"); } });
    } catch (e) {
        threw = e;
    }
    assert(threw instanceof RangeError, true);
}

/* a view that is already out of bounds when the call starts throws, unlike
   one that goes out of bounds during the coercion */
{
    const rab = new ArrayBuffer(16, { maxByteLength: 16 });
    const ta = new Int32Array(rab, 0, 4);
    rab.resize(8);

    for (const call of [() => ta.indexOf(1), () => ta.lastIndexOf(1),
                        () => ta.includes(1)]) {
        let threw = null;
        try {
            call();
        } catch (e) {
            threw = e;
        }
        assert(threw instanceof TypeError, true);
    }
}
