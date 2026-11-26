import { assert } from "./assert.js";

/* [[AsyncEvaluation]] must be cleared once a module has finished evaluating.
   A module that is only asynchronous because a dependency has a top-level
   await keeps the flag set otherwise, and the *next* module that imports it
   then treats a finished module as a pending async dependency: it registers
   itself in [[AsyncParentModules]] of a module that will never notify its
   parents again, so its evaluation promise never settles. */

/* Settling is decided against a bounded chain of microtask turns rather than
   a timer: a working import settles in a handful of turns, a wedged one
   never settles at all. */
function turns(n) {
    let p = Promise.resolve();
    for (let i = 0; i < n; i++)
        p = p.then(() => {});
    return p.then(() => "pending");
}

async function settles(promise) {
    return Promise.race([promise.then(v => ({ ok: v }), e => ({ err: e })),
                         turns(200)]);
}

/* the leaf has the top-level await, the parent only inherits its asynchrony */
{
    const parent = await settles(import("./fixture_async_sync_parent.js"));
    assert(parent.ok !== undefined, true, "parent import settled");
    assert(parent.ok.parent, 2);
}

/* importing the finished module again must not wedge */
{
    const again = await settles(import("./fixture_async_sync_parent.js"));
    assert(again.ok !== undefined, true, "re-import settled");
    assert(again.ok.parent, 2);
}

/* and neither must a fresh module that depends on it: this is the one that
   used to hang, because the finished parent still looked async */
{
    const grand = await settles(import("./fixture_async_grandparent.js"));
    assert(grand.ok !== undefined, true, "grandparent import settled");
    assert(grand.ok.grandparent, 3);
}

/* the leaf itself is fine too, before and after */
{
    const leaf = await settles(import("./fixture_async_leaf.js"));
    assert(leaf.ok !== undefined, true, "leaf import settled");
    assert(leaf.ok.leaf, 1);
}

/* a rejecting async module must clear the flag as well, so that a later
   importer of a *different* finished module is unaffected and the rejected
   one keeps reporting its error rather than hanging */
{
    const bad = await settles(import("./fixture_async_rejects.js"));
    assert(bad.err instanceof Error, true, "rejecting module settled");
    assert(bad.err.message, "leaf boom");

    const badAgain = await settles(import("./fixture_async_rejects.js"));
    assert(badAgain.err instanceof Error, true, "re-import settled");
    assert(badAgain.err.message, "leaf boom");

    /* a module importing the failed one reports the same failure, promptly */
    const importer = await settles(import("./fixture_async_rejects_parent.js"));
    assert(importer.err instanceof Error, true, "importer settled");
    assert(importer.err.message, "leaf boom");
}
