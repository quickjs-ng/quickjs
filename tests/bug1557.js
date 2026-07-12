// https://github.com/quickjs-ng/quickjs/issues/1557
import { assert, assertArrayEquals } from "./assert.js";

assertArrayEquals(
    Iterator.from([1, 2, 3, 4]).filter(x => x > 1).take(1).toArray(),
    [2]
);

assertArrayEquals(Iterator.from([1, 2, 3, 4]).map(x => x * 2).take(2).toArray(), [2, 4]);
assertArrayEquals(Iterator.from([1, 2, 3, 4, 5]).drop(1).take(2).toArray(), [2, 3]);
assertArrayEquals(Iterator.from([1, 2, 3]).flatMap(x => [x, x * 10]).take(3).toArray(), [1, 10, 2]);
assertArrayEquals(Iterator.from([1, 2, 3, 4, 5, 6]).filter(x => x % 2 === 0).map(x => x + 1).take(2).toArray(), [3, 5]);

// Calling `.return()` explicitly on a helper over a `return`-less source must
// not throw and must report completion with an undefined value.
for (const make of [
    it => it.map(x => x),
    it => it.filter(() => true),
    it => it.take(5),
    it => it.drop(0),
    it => it.flatMap(x => [x]),
]) {
    // `[][Symbol.iterator]()` is a plain array iterator: it has no `return`.
    const helper = make([1, 2, 3][Symbol.iterator]());
    const r1 = helper.return();
    assert(r1.done, true);
    assert(r1.value, undefined);
    // A second `.return()` is a no-op and also reports done.
    const r2 = helper.return();
    assert(r2.done, true);
    assert(r2.value, undefined);
}

// When the source *does* have a `return`, closing the helper must still
// forward to it exactly once.
for (const make of [
    it => it.map(x => x),
    it => it.filter(() => true),
    it => it.take(5),
    it => it.drop(0),
    it => it.flatMap(x => [x]),
]) {
    let returnCount = 0;
    class Src extends Iterator {
        next() { return { done: false, value: 1 }; }
        return(value) { returnCount++; return { done: true, value }; }
    }
    const helper = make(new Src());
    helper.next();
    assert(returnCount, 0);
    helper.return();
    assert(returnCount, 1);
    helper.return(); // already closed: not forwarded again
    assert(returnCount, 1);
}
