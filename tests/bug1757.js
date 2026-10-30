// https://github.com/quickjs-ng/quickjs/issues/1757
import { assert } from "./assert.js";

const error = new Error("boom");
const promise = Promise.resolve(1);
let calls = 0;
Object.defineProperty(promise, "constructor", {
    get() {
        calls++;
        throw error;
    },
});

const log = [];
const result = (async () => {
    for (let i = 0; i < 2; i++) {
        try {
            await promise;
            assert(false);
        } catch (e) {
            assert(e === error);
            log.push("catch");
        } finally {
            log.push("finally");
        }
    }
    assert(await Promise.resolve(42), 42);
    try {
        await promise;
        assert(false);
    } catch (e) {
        assert(e === error);
        log.push("resumed catch");
    }
    return 123;
})();

// PromiseResolve throws before Await suspends the execution context.
assert(log.join(","), "catch,finally,catch,finally");
assert(await result, 123);
assert(log.join(","), "catch,finally,catch,finally,resumed catch");
assert(calls, 3);

let finalized = false;
const rejected = (async () => {
    try {
        await promise;
    } finally {
        await 0;
        finalized = true;
    }
})();
let caught = false;
try {
    await rejected;
} catch (e) {
    assert(e === error);
    caught = true;
}
assert(caught);
assert(finalized);

assert(await (async () => {
    try {
        return await promise;
    } finally {
        return 456;
    }
})(), 456);
