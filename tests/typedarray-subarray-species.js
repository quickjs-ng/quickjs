import { assert } from "./assert.js";

/* %TypedArray%.prototype.subarray: the species constructor arguments are
   computed from the [[ByteOffset]] slot and must not be revalidated against
   the buffer, and a length-tracking result gets no length argument. */

/* detaching the buffer while coercing 'end' must not throw: the species
   constructor still receives the original byte offset */
{
    const ab = new ArrayBuffer(16);
    const ta = new Float64Array(ab, 8, 1);
    const marker = new Float64Array(0);
    let args;
    ta.constructor = {
        [Symbol.species]: function(...a) {
            args = a;
            return marker;
        }
    };
    const end = {
        valueOf() {
            ab.transfer();
            return 0;
        }
    };
    assert(ta.subarray(1, end), marker);
    assert(args.length, 3);
    assert(args[0], ab);
    assert(args[1], 16);
    assert(args[2], 0);
}

/* a length-tracking view with undefined 'end' calls the species constructor
   with two arguments so the result is length-tracking too */
{
    const rab = new ArrayBuffer(24, { maxByteLength: 32 });
    const ta = new Float64Array(rab);
    let args;
    ta.constructor = {
        [Symbol.species]: function(...a) {
            args = a;
            return new Float64Array(a[0], a[1]);
        }
    };
    const res = ta.subarray(1);
    assert(args.length, 2);
    assert(args[0], rab);
    assert(args[1], 8);
    rab.resize(32);
    assert(res.length, 3);
}

/* with an explicit 'end' the length argument is passed */
{
    const rab = new ArrayBuffer(24, { maxByteLength: 32 });
    const ta = new Float64Array(rab);
    let args;
    ta.constructor = {
        [Symbol.species]: function(...a) {
            args = a;
            return new Float64Array(a[0], a[1], a[2]);
        }
    };
    ta.subarray(1, 3);
    assert(args.length, 3);
    assert(args[2], 2);
}

/* without a species constructor, subarray of a detached typed array still
   throws a TypeError from the typed array constructor */
{
    const ab = new ArrayBuffer(16);
    const ta = new Float64Array(ab, 8);
    ab.transfer();
    let err;
    try {
        ta.subarray(0);
    } catch (e) {
        err = e;
    }
    assert(err instanceof TypeError, true);
}
