import { assert } from "./assert.js";

/* SortCompare calls comparefn for every pair it compares; there is no
   allowance for skipping the call when the two values happen to be the same
   value. Code that uses the comparator for its side effects depends on it. */

/* the jQuery uniqueSort() pattern: the comparator is handed the same object
   twice and records that it saw a duplicate */
{
    const o = {};
    let dups = false;
    const r = [o, o].sort((a, b) => { if (a === b) dups = true; return 0; });
    assert(dups, true);
    assert(r.length, 2);
    assert(r[0] === o, true);
    assert(r[1] === o, true);
}

/* identical primitives get the same treatment */
{
    const seen = [];
    [1, 1].sort((a, b) => { seen.push(a, b); return 0; });
    assert(seen.length, 2);
    assert(seen[0], 1);
    assert(seen[1], 1);
}

/* toSorted() sorts through the same path */
{
    const o = {};
    let dups = false;
    const r = [o, o].toSorted((a, b) => { if (a === b) dups = true; return 0; });
    assert(dups, true);
    assert(r.length, 2);
    assert(r[0] === o, true);
}

/* an exception from the comparator is not swallowed for identical values */
{
    const o = {};
    let err;
    try {
        [o, o].sort(() => { throw new RangeError("boom"); });
    } catch (e) {
        err = e;
    }
    assert(err instanceof RangeError, true);
    assert(err.message, "boom");
}

/* the same holds beyond the insertion sort cutoff: every comparison of the
   all-identical array reaches the comparator */
{
    const o = {};
    let calls = 0;
    const r = new Array(100).fill(o).sort((x, y) => {
        if (x === o && y === o) calls++;
        return 0;
    });
    assert(calls >= 99, true); /* at least one comparison per element */
    assert(r.length, 100);
    assert(r.every(v => v === o), true);
}

/* the shortcut compared the two values bit for bit, so it caught every type
   whose JSValue is the value itself or a shared pointer, not just objects */
{
    function calls(arr) {
        let n = 0;
        Array.prototype.sort.call(arr, () => { n++; return 0; });
        return n;
    }

    const s = "abc";
    const sym = Symbol("s");
    const o = {};
    assert(calls([s, s]), 1, "same string");
    assert(calls([NaN, NaN]), 1, "same NaN");
    assert(calls([1n, 1n]), 1, "same bigint");
    assert(calls([sym, sym]), 1, "same symbol");
    assert(calls([true, true]), 1, "same boolean");
    assert(calls([null, null]), 1, "same null");
    assert(calls([o, o]), 1, "same object");
    assert(calls([o, o, o]), 2, "three identical objects");

    /* an array-like sorted through .call() takes the same path */
    assert(calls({ length: 2, 0: o, 1: o }), 1, "array-like");
}

/* undefined and holes are still sorted to the end without ever reaching the
   comparator: that is SortCompare's own rule, not the shortcut */
{
    let n = 0;
    const cmp = () => { n++; return 0; };

    n = 0;
    assert([undefined, undefined].sort(cmp).length, 2);
    assert(n, 0, "two undefined");

    n = 0;
    const mixed = [undefined, 1].sort(cmp);
    assert(n, 0, "undefined and a value");
    assert(mixed[0], 1);
    assert(mixed[1], undefined);

    n = 0;
    const holes = new Array(3);
    holes[0] = 1;
    holes.sort(cmp);
    assert(n, 0, "holes");
    assert(holes[0], 1);
    assert(1 in holes, false);
}

/* typed arrays sort through a different comparison function that never had
   the shortcut; it must keep calling the comparator too */
{
    for (const Ctor of [Int8Array, Uint8Array, Int32Array, Float64Array]) {
        let n = 0;
        const t = new Ctor([1, 1, 1]);
        t.sort(() => { n++; return 0; });
        assert(n, 2, Ctor.name);

        n = 0;
        new Ctor([1, 1, 1]).toSorted(() => { n++; return 0; });
        assert(n, 2, Ctor.name + " toSorted");
    }

    /* including a bigint typed array */
    let n = 0;
    new BigInt64Array([1n, 1n, 1n]).sort(() => { n++; return 0; });
    assert(n, 2, "BigInt64Array");
}
