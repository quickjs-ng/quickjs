// https://github.com/quickjs-ng/quickjs/issues/1693
import { assert, assertThrows } from "./assert.js";

class NextGetterError extends Error {}

assertThrows(NextGetterError, () => Iterator.from({
    [Symbol.iterator]() {
        return {
            get next() {
                throw new NextGetterError();
            },
        };
    },
}));

let nextGets = 0;
let nextCalls = 0;
const inner = {
    get next() {
        nextGets++;
        return function () {
            nextCalls++;
            return { value: nextCalls, done: nextCalls > 2 };
        };
    },
};
const wrapped = Iterator.from({
    [Symbol.iterator]() {
        return inner;
    },
});

assert(wrapped === inner, false);
assert(wrapped instanceof Iterator, true);
assert(nextGets, 1);
assert(wrapped.next().value, 1);
assert(wrapped.next().value, 2);
assert(wrapped.next().done, true);
assert(nextGets, 1);
assert(nextCalls, 3);

class RedirectingIterator extends Iterator {
    next() {
        return { done: true };
    }
    [Symbol.iterator]() {
        return inner;
    }
}

const redirected = Iterator.from(new RedirectingIterator());
assert(redirected instanceof Iterator, true);
assert(redirected === inner, false);
assert(redirected instanceof RedirectingIterator, false);
assert(nextGets, 2);

let instanceNextGets = 0;
let instanceIteratorGets = 0;
let instanceIteratorCalls = 0;
class ExistingIterator extends Iterator {
    get [Symbol.iterator]() {
        instanceIteratorGets++;
        return function () {
            instanceIteratorCalls++;
            return this;
        };
    }
    get next() {
        instanceNextGets++;
        return function () {
            return { done: true };
        };
    }
}

const existing = new ExistingIterator();
assert(Iterator.from(existing) === existing, true);
assert(instanceIteratorGets, 1);
assert(instanceIteratorCalls, 1);
assert(instanceNextGets, 1);
