import { assert } from "./assert.js";

/* Object.groupBy and Map.groupBy call the callback with undefined as the
   this value, convert an Object.groupBy key with ToPropertyKey once, and
   close the iterator when the callback or the key conversion throws. */

for (const groupBy of [Object.groupBy, Map.groupBy]) {
    let receiver = null;
    groupBy([1], function () { "use strict"; receiver = this; return 0; });
    assert(receiver, undefined);

    let closed = 0;
    const iterable = {
        [Symbol.iterator]() {
            return {
                next() { return { value: 1, done: false }; },
                return() { closed++; return {}; },
            };
        },
    };
    let caught = null;
    try {
        groupBy(iterable, () => { throw "callback"; });
    } catch (e) {
        caught = e;
    }
    assert(caught, "callback");
    assert(closed, 1);
}

{
    let calls = 0;
    const key = { toString() { calls++; return "k"; } };
    const groups = Object.groupBy([1, 2], () => key);
    assert(calls, 2);
    assert(groups.k.length, 2);

    let closed = 0;
    const iterable = {
        [Symbol.iterator]() {
            return {
                next() { return { value: 1, done: false }; },
                return() { closed++; return {}; },
            };
        },
    };
    const bad = { toString() { throw "key"; } };
    let caught = null;
    try {
        Object.groupBy(iterable, () => bad);
    } catch (e) {
        caught = e;
    }
    assert(caught, "key");
    assert(closed, 1);
}
