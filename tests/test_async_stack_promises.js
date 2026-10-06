import { assert, assertArrayEquals } from "./assert.js";

const prepare = (_, frames) => frames;
Error.prepareStackTrace = prepare;

async function caught(promise) {
    try {
        await promise;
    } catch (error) {
        return error instanceof AggregateError ? error.errors[0] : error;
    }
    throw new Error("expected rejection");
}

async function fail() {
    await 0;
    throw new Error("failed");
}

async function all() { await Promise.all([Promise.resolve(1), fail()]); }
async function any() { await Promise.any([fail(), fail()]); }
async function allSettled() {
    const results = await Promise.allSettled([Promise.resolve(1), fail()]);
    throw results[1].reason;
}
async function nested() { await Promise.all([Promise.any([fail()])]); }
async function chained() { await fail().then(value => value); }
async function longChain() {
    let promise = fail();
    for (let i = 0; i < 100; i++)
        promise = promise.then(value => value);
    await promise;
}
async function callback() {
    await Promise.resolve().then(function onFulfilled() {
        throw new Error("failed");
    });
}
async function rejection() {
    await Promise.reject(1).catch(function onRejected() {
        throw new Error("failed");
    });
}
async function cleanup() {
    await Promise.resolve().finally(function onFinally() {
        throw new Error("failed");
    });
}
async function thenable() {
    await { then: function resolveThenable() { throw new Error("failed"); } };
}
async function delayedFail() {
    await 0;
    await 0;
    throw new Error("failed");
}
async function adopted() { return delayedFail(); }
async function adoption() { await adopted(); }
async function callbackAsync() {
    await Promise.resolve().then(async function insideCallback() {
        throw new Error("failed");
    });
}

async function check(run, origin, combinators = []) {
    const error = await caught(run());
    assert(error instanceof Error, true, run.name);
    assert(error.message, "failed", run.name);
    const synthetic = error.stack.filter(frame => frame.getPromiseIndex() !== null);
    assertArrayEquals(synthetic.map(frame =>
        [frame.getFunctionName(), frame.getPromiseIndex()].join(":")), combinators);
    for (const frame of synthetic) {
        assert(frame.isAsync());
        assert(!frame.isNative());
        assert(!frame.isConstructor());
        assert(frame.isPromiseAll(), frame.getFunctionName() === "all");
        assert(frame.getFileName(), null);
        assert(frame.getLineNumber(), null);
        assert(frame.getColumnNumber(), null);
    }
    // Check real JavaScript frames separately from synthetic combinator frames.
    const frames = error.stack.filter(frame => !frame.isNative() && frame.getFileName() &&
        frame.getPromiseIndex() === null);
    assert(frames.slice(0, 3).map(frame => frame.getFunctionName()).join(","),
        [origin, run.name, "caught"].join(","), run.name);
    assertArrayEquals(frames.slice(0, 3).map(frame => frame.isAsync()),
        [false, true, true]);
    for (const frame of frames.slice(0, 3)) {
        assert(frame.getFileName().endsWith("test_async_stack_promises.js"));
        assert(frame.getLineNumber() > 0);
        assert(frame.getColumnNumber() > 0);
    }
    // Exercise the same cases with QuickJS's default string formatting.
    Error.prepareStackTrace = undefined;
    const text = (await caught(run())).stack;
    assert(text.includes("at async " + run.name + " ("), true, run.name);
    for (const combinator of combinators) {
        const [name, index] = combinator.split(":");
        assert(text.includes("at async Promise." + name + " (index " + index + ")"));
    }
    Error.prepareStackTrace = prepare;
}

await check(all, "fail", ["all:1"]);
await check(any, "fail", ["any:0"]);
await check(allSettled, "fail", ["allSettled:1"]);
await check(nested, "fail", ["any:0", "all:0"]);
await check(chained, "fail");
await check(longChain, "fail");
await check(callback, "onFulfilled");
await check(rejection, "onRejected");
await check(cleanup, "onFinally");
await check(thenable, "resolveThenable");
await check(adoption, "delayedFail");
await check(callbackAsync, "insideCallback");

// User-defined promise species must not be called by stack reconstruction.
let resolvingCalls = 0;
class CustomPromise extends Promise {
    constructor(executor) {
        super((resolve, reject) => executor(
            value => { resolvingCalls++; resolve(value); }, reject));
    }
}
let captured;
await CustomPromise.resolve().then(() => {
    const before = resolvingCalls;
    captured = new Error("custom").stack;
    assert(resolvingCalls, before);
});
assert(Array.isArray(captured));

// An unrelated microtask must not inherit the preceding promise job's callers.
await new Promise(resolve => {
    Promise.resolve().then(() => {
        queueMicrotask(function unrelated() {
            captured = new Error("microtask").stack;
            resolve();
        });
    });
});
assert(!captured.some(frame => frame.isAsync()));

for (const limit of [0, 1, 2, 3]) {
    Error.stackTraceLimit = limit;
    const error = await caught(all());
    assert(error instanceof Error);
    assert(error.stack.length, limit);
    if (limit >= 2) {
        assert(error.stack[1].isPromiseAll());
        assert(error.stack[1].getPromiseIndex(), 1);
    }
    if (limit >= 3) {
        assert(error.stack[2].getPromiseIndex(), null);
        assert(!error.stack[2].isPromiseAll());
    }
}
Error.stackTraceLimit = 10;
Error.prepareStackTrace = undefined;
