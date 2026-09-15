import { assert } from "./assert.js";

/* AsyncModuleExecutionRejected rejects the module's own [[TopLevelCapability]]
   before recursing into [[AsyncParentModules]], so a failing async module
   settles leaf first and root last. Doing it the other way around settles the
   whole chain in reverse. */

/* Importing the leaf first, then each ancestor in turn, gives every module in
   the chain its own top level capability, which is what makes the order
   observable at all. */
const order = [];
const errors = [];

function record(name) {
    return e => { order.push(name); errors.push(e); };
}

const leaf = import("./fixture_reject_leaf.js").then(record("leaf.ok"),
                                                     record("leaf"));
const mid = import("./fixture_reject_mid.js").then(record("mid.ok"),
                                                   record("mid"));
const top = import("./fixture_reject_top.js").then(record("top.ok"),
                                                   record("top"));

await Promise.all([leaf, mid, top]);

assert(order.join(" -> "), "leaf -> mid -> top");

/* every module in the chain reports the leaf's error, and it is the very
   same object, not a copy */
assert(errors.length, 3);
for (const e of errors) {
    assert(e instanceof Error, true);
    assert(e.message, "leaf rejected");
    assert(e === errors[0], true);
}

/* re-importing the failed modules keeps reporting the same error and does
   not re-run anything */
{
    const again = [];
    await Promise.all([
        import("./fixture_reject_leaf.js").then(() => again.push("ok"),
                                                e => again.push(e.message)),
        import("./fixture_reject_top.js").then(() => again.push("ok"),
                                               e => again.push(e.message)),
    ]);
    assert(again.length, 2);
    assert(again[0], "leaf rejected");
    assert(again[1], "leaf rejected");
}

/* two independent ancestors of the same failing leaf are both notified */
{
    const seen = [];
    const a = import("./fixture_reject_sib_a.js").catch(e => seen.push("a"));
    const b = import("./fixture_reject_sib_b.js").catch(e => seen.push("b"));
    await Promise.all([a, b]);
    seen.sort();
    assert(seen.join(","), "a,b");
}
