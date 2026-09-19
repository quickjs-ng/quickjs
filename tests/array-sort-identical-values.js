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

/* now that identical values reach the comparator too, there are more calls
   from which user code can reach back into the array being sorted */
{
    /* shrinking the array from the comparator */
    {
        const a = [3, 1, 3, 1, 3, 1, 3, 1];
        let calls = 0;
        a.sort((x, y) => {
            calls++;
            if (calls === 2) a.length = 3;
            return x - y;
        });
        assert(calls > 0, true);
        /* sort collects the elements before it compares any of them, so the
           truncation is undone when the sorted list is written back */
        assert(a.length, 8);
        assert(a.join(","), "1,1,1,1,3,3,3,3");
    }

    /* growing it */
    {
        const a = [2, 2, 2, 2];
        let calls = 0;
        a.sort((x, y) => {
            if (++calls === 1) a.push(1, 1);
            return x - y;
        });
        assert(a.length, 6);
        assert(a.every(v => v === 1 || v === 2), true);
    }

    /* deleting elements, which turns them into holes that sort to the end */
    {
        const a = [1, 1, 1, 1, 1];
        a.sort((x, y) => {
            delete a[4];
            return x - y;
        });
        assert(a.length, 5);
    }

    /* reversing it under the sort's feet */
    {
        const a = [1, 1, 2, 2, 3, 3];
        let calls = 0;
        const out = a.sort((x, y) => {
            if (++calls === 3) a.reverse();
            return x - y;
        });
        assert(out, a);
        assert(a.length, 6);
    }

    /* a comparator that sorts the same array again */
    {
        const a = [2, 2, 1, 1];
        let depth = 0;
        a.sort(function cmp(x, y) {
            if (depth === 0) {
                depth++;
                a.slice().sort(cmp);
                depth--;
            }
            return x - y;
        });
        assert(a.join(","), "1,1,2,2");
    }
}

/* the comparator's return value is coerced with ToNumber, and anything that
   is not less than or greater than zero leaves the order alone */
{
    const mk = () => [{ i: 0 }, { i: 1 }, { i: 2 }, { i: 3 }];
    const order = a => a.map(v => v.i).join(",");

    assert(order(mk().sort(() => NaN)), "0,1,2,3");
    assert(order(mk().sort(() => undefined)), "0,1,2,3");
    assert(order(mk().sort(() => "")), "0,1,2,3");
    assert(order(mk().sort(() => null)), "0,1,2,3");
    assert(order(mk().sort(() => -0)), "0,1,2,3");
    assert(order(mk().sort(() => "0")), "0,1,2,3");
    assert(order(mk().sort(() => false)), "0,1,2,3");
    assert(order(mk().sort(() => 0.5)), "3,2,1,0");
    assert(order(mk().sort(() => "-1")), "0,1,2,3");

    /* a comparator that is not callable is a TypeError before any call */
    for (const bad of [null, 1, "x", true, {}, Symbol()]) {
        let threw = false;
        try {
            mk().sort(bad);
        } catch (e) {
            threw = e instanceof TypeError;
        }
        assert(threw, true, String(bad));
    }
    /* undefined means the default comparator, and is not an error */
    assert(mk().sort(undefined).length, 4);
}

/* a long run of values the shortcut used to skip entirely still sorts, is
   still stable, and calls the comparator for every comparison it makes */
{
    const n = 2000;
    const a = [];
    for (let i = 0; i < n; i++)
        a.push({ key: i % 3, i });
    let calls = 0;
    a.sort((x, y) => { calls++; return x.key - y.key; });
    assert(calls > 0, true);
    assert(a.length, n);
    for (let i = 1; i < n; i++) {
        assert(a[i - 1].key <= a[i].key, true, `order at ${i}`);
        if (a[i - 1].key === a[i].key)
            assert(a[i - 1].i < a[i].i, true, `stability at ${i}`);
    }

    /* the same array where every element is the identical object */
    const same = {};
    const b = new Array(n).fill(same);
    let same_calls = 0;
    b.sort(() => { same_calls++; return 0; });
    assert(same_calls > 0, true);
    assert(b.length, n);
    assert(b.every(v => v === same), true);
}
