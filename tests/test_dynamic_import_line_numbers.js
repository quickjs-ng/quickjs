import { assert } from "./assert.js";

/* A stack frame in a function that calls import() must point at the
   import() call, not at the start of the statement, an earlier call in the
   same function or the function itself. import() can run user code
   synchronously, such as a getter on the options object, which captures the
   frame while import() executes. It can also reject without a stack, for
   example on a bad options argument, and the TypeError then gets its stack
   when the await rethrows it. */

async function import_in_declaration()
{
    const m = await import("./assert.js", { with: 1 });
}

async function import_after_call()
{
    Math.max(0);
    const m = await import("./assert.js", { with: 1 });
}

async function import_in_statement()
{
    await import("./assert.js", { with: 1 });
}

function import_with_throwing_getter()
{
    Math.max(0);
    return import("./assert.js", { get with() { throw new Error("with"); } });
}

async function location(f)
{
    const e = await f().then(() => null, e => e);
    assert(e instanceof Error, true, f.name);
    const frame = e.stack.find(c => c.getFunctionName() === f.name);
    assert(frame !== undefined, true, f.name);
    return [frame.getLineNumber(), frame.getColumnNumber()];
}

Error.prepareStackTrace = (_, frames) => frames;
try {
    assert(await location(import_in_declaration), [13, 21]);
    assert(await location(import_after_call), [19, 21]);
    assert(await location(import_in_statement), [24, 11]);
    assert(await location(import_with_throwing_getter), [30, 12]);
} finally {
    Error.prepareStackTrace = undefined;
}
