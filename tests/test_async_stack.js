import { assert, assertArrayEquals } from "./assert.js";

async function leaf(capture = false) {
    await 0;
    const error = capture ? {} : new Error("boom");
    if (capture)
        Error.captureStackTrace(error);
    throw Object.freeze(error);
}

async function middle(capture) {
    await leaf(capture);
}

async function outer(capture) {
    await middle(capture);
}

async function caught(promise) {
    try {
        await promise;
    } catch (error) {
        return error;
    }
    throw new Error("expected rejection");
}

function check(frames, names) {
    frames = frames.filter(f => !f.isNative());
    assertArrayEquals(frames.slice(0, names.length).map(f => f.getFunctionName()), names);
    for (let i = 0; i < names.length; i++) {
        assert(frames[i].isAsync(), i !== 0);
        assert(!frames[i].isNative());
        assert(!frames[i].isConstructor());
        assert(frames[i].getFileName().endsWith("test_async_stack.js"));
        assert(frames[i].getLineNumber() > 0);
        assert(frames[i].getColumnNumber() > 0);
    }
}

let calls = 0;
Error.prepareStackTrace = (_, frames) => {
    calls++;
    return Object.freeze(frames);
};
let error = await caught(outer());
check(error.stack, ["leaf", "middle", "outer"]);
assert(error.stack[1].getFunction(), middle);
assert(error.stack[2].getFunction(), outer);
assert(error.stack[1].getLineNumber(), 12);
assert(error.stack[2].getLineNumber(), 16);
assert(calls, 1);

// Capturing on a plain object must work before any rejection is propagated.
check((await caught(outer(true))).stack, ["leaf", "middle", "outer"]);
assert(calls, 2, "capture on plain object");

// Rethrowing an existing error must not modify its original stack.
const stack = error.stack;
let cleaned = false;
async function rethrow() {
    try {
        await 0;
        throw error;
    } catch (caught) {
        await 0;
        throw caught;
    } finally {
        await 0;
        cleaned = true;
    }
}
assert(await caught(rethrow()) === error);
assert(error.stack, stack);
assert(cleaned);

// Callers that have not suspended are already on the synchronous stack.
async function immediate() { throw new Error("sync"); }
async function synchronous() { await immediate(); }
const sync = (await caught(synchronous())).stack;
assert(sync.filter(f => f.getFunctionName() === "synchronous").length, 1);
assert(!sync.find(f => f.getFunctionName() === "synchronous").isAsync());

// Follow both an async generator's await and its consumer's next() promise.
async function* generator() { await leaf(); }
async function consumer() { await generator().next(); }
check((await caught(consumer())).stack, ["leaf", "generator", "consumer"]);
async function* throwingGenerator() { yield 1; await 0; throw new Error("generator"); }
let received = 0;
async function iterate() { for await (const value of throwingGenerator()) received += value; }
check((await caught(iterate())).stack, ["throwingGenerator", "iterate"]);
assert(received, 1);
// A completed generator's return() handler must not expose its freed frame.
async function* empty() {}
const completed = empty();
await completed.next();
assert((await caught(completed.return(leaf()))).stack[0].getFunctionName(), "leaf");

// A promise with multiple consumers has no single async caller.
const shared = leaf();
async function waiter() { await shared; }
const errors = await Promise.all([caught(waiter()), caught(waiter())]);
assert(errors[0], errors[1]);
assert(errors[0].stack.length, 1);

// Stack limits apply to synchronous and asynchronous frames together.
for (const limit of [0, 1, 2]) {
    Error.stackTraceLimit = limit;
    assert((await caught(outer())).stack.length, limit);
}
async function recursive(n) {
    await 0;
    if (n) return await recursive(n - 1);
    throw new Error("deep");
}
Error.stackTraceLimit = Infinity;
const deepError = await caught(recursive(100));
assert(deepError.message, "deep");
const deep = deepError.stack;
assert(deep.length, 64);
assert(deep.slice(1).every(f => f.isAsync()));

Error.prepareStackTrace = undefined;
Error.stackTraceLimit = 10;
const text = (await caught(outer())).stack;
assert(text.includes("    at leaf ("));
assert(text.includes("    at async middle ("));
assert(text.includes("    at async outer ("));
Error.stackTraceLimit = 1;
assert(!(await caught(outer())).stack.includes("at async"));
Error.stackTraceLimit = 10;

// Propagation must not invoke user-defined stack accessors.
error = new Error("custom");
let reads = 0;
Object.defineProperty(error, "stack", { get() { reads++; return "custom"; } });
assert(await caught(rethrow()), error);
assert(reads, 0);

// Discarded structured frames must not keep a closure/error cycle alive.
Error.prepareStackTrace = () => "custom";
(function () {
    let retained;
    function capture() { retained = new Error("cycle"); }
    capture();
})();
Error.prepareStackTrace = undefined;
