import { assert } from "./assert.js";

/* Array.fromAsync must reject when next() returns a non-object, and
   must read next only once. */

async function rejects(iterable) {
    try {
        await Array.fromAsync(iterable);
    } catch (e) {
        return e instanceof TypeError;
    }
    return false;
}

for (const result of [0, undefined, null, "x"]) {
    let calls = 0;
    const next = () => {
        if (++calls > 2) throw "next called too often";
        return result;
    };
    assert(await rejects({ [Symbol.asyncIterator]: () => ({ next }) }), true);
    assert(await rejects({ [Symbol.iterator]: () => ({ next }) }), true);
    assert(calls, 2);
}

{
    let gets = 0, i = 0;
    const iter = {
        get next() {
            gets++;
            return async () => ({ value: i, done: i++ >= 3 });
        },
    };
    const arr = await Array.fromAsync({ [Symbol.asyncIterator]: () => iter });
    assert(arr.join(), "0,1,2");
    assert(gets, 1);
}
