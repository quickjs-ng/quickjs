import { assert } from "./assert.js";

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
    assert(await location(import_in_declaration), [5, 21]);
    assert(await location(import_after_call), [11, 21]);
    assert(await location(import_in_statement), [16, 11]);
    assert(await location(import_with_throwing_getter), [22, 12]);
} finally {
    Error.prepareStackTrace = undefined;
}
