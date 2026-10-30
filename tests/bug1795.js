// https://github.com/quickjs-ng/quickjs/issues/1795
import { assert, assertArrayEquals } from "./assert.js";

// 1. Array.fromAsync must not look up .call on mapper function (iterable source).
{
    function mapper(x) { return x * 10; }
    mapper.call = function () { return "hijacked"; };
    const res = await Array.fromAsync([1, 2], mapper);
    assertArrayEquals(res, [10, 20]);
}

// 2. Array.fromAsync must not look up .call on mapper function (array-like source).
{
    function mapper(x) { return x * 10; }
    mapper.call = function () { return "hijacked"; };
    const res = await Array.fromAsync({ length: 2, 0: 1, 1: 2 }, mapper);
    assertArrayEquals(res, [10, 20]);
}

// 3. Array.fromAsync must not look up .call on iterator method ([Symbol.iterator]).
{
    const method = function () { return [1, 2].values(); };
    method.call = function () { return ["hijacked"].values(); };
    const res = await Array.fromAsync({ [Symbol.iterator]: method });
    assertArrayEquals(res, [1, 2]);
}

// 4. Array.fromAsync must not look up .call on async iterator method ([Symbol.asyncIterator]).
{
    const asyncMethod = async function* () { yield 1; yield 2; };
    asyncMethod.call = function () { throw new Error("hijacked"); };
    const res = await Array.fromAsync({ [Symbol.asyncIterator]: asyncMethod });
    assertArrayEquals(res, [1, 2]);
}

// 5. Array.fromAsync works even if mapper.call or method.call is not callable (null or primitive).
{
    const mapper = (x) => x + 1;
    mapper.call = null;
    const res = await Array.fromAsync([10, 20], mapper);
    assertArrayEquals(res, [11, 21]);
}
{
    const method = function* () { yield 3; yield 4; };
    method.call = 123;
    const res = await Array.fromAsync({ [Symbol.iterator]: method });
    assertArrayEquals(res, [3, 4]);
}

// 6. Verify thisArg and index parameters are passed correctly to mapper.
{
    const thisObj = { multiplier: 3 };
    const indices = [];
    const thisReceivers = [];
    function mapper(val, idx) {
        indices.push(idx);
        thisReceivers.push(this);
        return val * this.multiplier;
    }
    mapper.call = null;
    const res = await Array.fromAsync([5, 6], mapper, thisObj);
    assertArrayEquals(res, [15, 18]);
    assertArrayEquals(indices, [0, 1]);
    assert(thisReceivers[0], thisObj);
    assert(thisReceivers[1], thisObj);
}
{
    const thisObj = { multiplier: 4 };
    const indices = [];
    const thisReceivers = [];
    function mapper(val, idx) {
        indices.push(idx);
        thisReceivers.push(this);
        return val * this.multiplier;
    }
    mapper.call = null;
    const res = await Array.fromAsync({ length: 2, 0: 5, 1: 6 }, mapper, thisObj);
    assertArrayEquals(res, [20, 24]);
    assertArrayEquals(indices, [0, 1]);
    assert(thisReceivers[0], thisObj);
    assert(thisReceivers[1], thisObj);
}

// 7. Verify receiver (this) for iterator method is the arrayLike object.
{
    let receivedThis = null;
    const obj = {
        [Symbol.iterator]() {
            receivedThis = this;
            return [7, 8].values();
        }
    };
    obj[Symbol.iterator].call = null;
    const res = await Array.fromAsync(obj);
    assertArrayEquals(res, [7, 8]);
    assert(receivedThis, obj);
}
