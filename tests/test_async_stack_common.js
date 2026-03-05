import { assert, assertArrayEquals } from "./assert.js";

// Keep these common scenarios runnable in both QuickJS and V8/Node.
Error.prepareStackTrace = (_, frames) => frames;

async function caught(promise) {
    try {
        await promise;
    } catch (error) {
        return error;
    }
    throw new Error("expected rejection");
}

function check(error, expected) {
    const frames = error.stack.filter(frame => !frame.isNative());
    assertArrayEquals(
        frames.slice(0, expected.length).map(frame =>
            [frame.getFunctionName(), frame.isAsync()].join(":")),
        expected.map(frame => frame.join(":")));
}

// Validation after several awaits retains both synchronous and async method frames.
function validate() { throw new Error("invalid response"); }
class Repository {
    async query() {
        await Promise.resolve();
        await new Promise(resolve => Promise.resolve().then(resolve));
        validate();
    }
    async findUser() { return await this.query(); }
}
async function handleRequest() { await new Repository().findUser(); }
const invalid = await caught(handleRequest());
assert(invalid.message, "invalid response");
check(invalid, [
    ["validate", false],
    ["query", false],
    ["findUser", true],
    ["handleRequest", true],
]);

// Capture a plain object's stack before throwing, filtering out the helper.
function capture() {
    const target = {};
    Error.captureStackTrace(target, capture);
    return target;
}
async function readConfig() {
    await 0;
    return capture();
}
async function start() { return await readConfig(); }
check(await start(), [["readConfig", false], ["start", true]]);

// Concurrent tasks retain their own direct-await chains, without mixing frames.
async function task(id) {
    await 0;
    await 0;
    throw new Error(String(id));
}
async function worker(id) { await task(id); }
const errors = await Promise.all([caught(worker(1)), caught(worker(2))]);
assert(errors[0] !== errors[1]);
for (let i = 0; i < errors.length; i++) {
    assert(errors[i].message, String(i + 1));
    check(errors[i], [["task", false], ["worker", true]]);
}

Error.prepareStackTrace = undefined;
