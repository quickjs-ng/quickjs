import { assert } from "./assert.js";

// Every element takes the non-matching path (the predicate is never true).
{
    const o = {};
    assert([o, o, o].values().find(() => false), undefined);
    assert(new Array(1000).fill(o).values().find(() => false), undefined);
}

// The match path still returns the first match and forwards (value, index).
{
    const seen = [];
    const r = [10, 20, 30, 40].values().find((v, i) => { seen.push(v, i); return v === 30; });
    assert(r, 30);
    assert(seen.length, 6); // stopped at the third element
    assert(seen[0], 10); assert(seen[1], 0);
    assert(seen[4], 30); assert(seen[5], 2);
}
