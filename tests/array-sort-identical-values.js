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
