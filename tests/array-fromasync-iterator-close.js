import { assert } from "./assert.js";

/* Array.fromAsync iterator close semantics:
   - a normal completion (done: true) must not call return()
   - an abrupt completion from the mapper (or an element promise rejection)
     must close the iterator, and the close must be awaited before the
     Array.fromAsync promise settles */

/* return() is not called when the iterator is exhausted normally */
{
    let returnCalled = false;
    const iter = {
        [Symbol.asyncIterator]() {
            let i = 0;
            return {
                async next() {
                    return i < 2 ? { value: i++, done: false }
                                 : { value: undefined, done: true };
                },
                async return() {
                    returnCalled = true;
                    return { done: true };
                },
            };
        },
    };
    await Array.fromAsync(iter);
    assert(returnCalled, false);
}

/* a throwing mapper closes the iterator */
{
    let returnCalled = false;
    const iter = {
        [Symbol.asyncIterator]() {
            return {
                async next() {
                    return { value: 1, done: false };
                },
                async return() {
                    returnCalled = true;
                    return { done: true };
                },
            };
        },
    };
    let error;
    try {
        await Array.fromAsync(iter, () => { throw new Error("boom"); });
    } catch (e) {
        error = e;
    }
    assert(error.message, "boom");
    assert(returnCalled, true);
}

/* the close is awaited: return()'s promise settles before fromAsync's */
{
    const order = [];
    const iter = {
        [Symbol.asyncIterator]() {
            return {
                async next() {
                    return { value: 1, done: false };
                },
                return() {
                    order.push("return");
                    return new Promise((resolve) => {
                        Promise.resolve().then(() => {
                            order.push("closed");
                            resolve({ done: true });
                        });
                    });
                },
            };
        },
    };
    await Array.fromAsync(iter, () => { throw new Error("boom"); })
        .catch(() => order.push("rejected"));
    assert(order.join(","), "return,closed,rejected");
}

/* a rejected return() does not mask the original error */
{
    const iter = {
        [Symbol.asyncIterator]() {
            return {
                async next() {
                    return { value: 1, done: false };
                },
                async return() {
                    throw new Error("close failed");
                },
            };
        },
    };
    let error;
    try {
        await Array.fromAsync(iter, () => { throw new Error("original"); });
    } catch (e) {
        error = e;
    }
    assert(error.message, "original");
}
