import { assertThrows } from "./assert.js";

function chain(f) {
    let it = [1, 2].values();
    for (let i = 0; i < 100000; i++)
        it = f(it);
    return it;
}

assertThrows(RangeError, () => chain(it => it.map(x => x)).next());
assertThrows(RangeError, () => chain(it => it.filter(x => true)).next());
assertThrows(RangeError, () => chain(it => it.take(1)).next());
assertThrows(RangeError, () => chain(it => it.drop(0)).next());
assertThrows(RangeError, () => chain(it => it.flatMap(x => [x])).next());

// %WrapForValidIteratorPrototype%.next
assertThrows(RangeError, () => chain(it => {
    const next = it.next;
    Object.setPrototypeOf(it, null);
    it.next = next;
    return Iterator.from(it);
}).next());
