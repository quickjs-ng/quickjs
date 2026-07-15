import { assert } from "./assert.js";

// The predicate rejects every value: each produced object must be freed.
{
    const src = Array.from({ length: 100 }, (_, i) => ({ i }));
    const out = src.values().filter(() => false).toArray();
    assert(out.length, 0);
}

// The predicate throws mid-iteration: the value being tested must be freed.
{
    let threw = false;
    try {
        [{ a: 1 }, { b: 2 }, { c: 3 }].values()
            .filter(() => { throw new Error("boom"); })
            .toArray();
    } catch (e) {
        threw = true;
    }
    assert(threw, true);
}
