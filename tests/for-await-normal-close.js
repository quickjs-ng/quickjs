import { assert } from "./assert.js";

function makeAsyncIter(log) {
    let i = 0;
    return { [Symbol.asyncIterator]() { return {
        next() {
            return Promise.resolve(i < 2 ? { value: i++, done: false }
                                          : { value: undefined, done: true });
        },
        return() { log.returned++; return Promise.resolve({ done: true }); },
    }; } };
}

/* normal completion must NOT call return() */
{
    const log = { returned: 0 };
    for await (const x of makeAsyncIter(log)) {}
    assert(log.returned, 0);
}

/* break is an abrupt completion and must call return() */
{
    const log = { returned: 0 };
    for await (const x of makeAsyncIter(log)) break;
    assert(log.returned, 1);
}

/* throw is an abrupt completion and must call return() */
{
    const log = { returned: 0 };
    try {
        for await (const x of makeAsyncIter(log)) throw new Error("stop");
    } catch (e) {}
    assert(log.returned, 1);
}

/* leaving the loop via break must await the result of return() */
{
    const order = [];
    const it = { [Symbol.asyncIterator]() { let i = 0; return {
        next() { return Promise.resolve({ value: i++, done: false }); },
        return() {
            order.push("return");
            return Promise.resolve().then(() => { order.push("resolved"); return { done: true }; });
        },
    }; } };
    for await (const x of it) break;
    order.push("after");
    assert(order.join(","), "return,resolved,after");
}
