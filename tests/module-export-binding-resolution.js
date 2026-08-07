import { assert } from "./assert.js";

/* ResolveExport only reports an ambiguity when two `export *` paths reach two
   *different* bindings. Reaching the same one twice is fine, and there are two
   ways to lose track of that:

   - `import { foo } from "m"; export { foo }` is not a local binding of this
     module. ParseModule turns it into an indirect export through "m", so it
     resolves to the same binding as `export { foo } from "m"`.
   - `export * as ns from "m"` resolves to m's namespace, which does not depend
     on which module re-exported it, so two such re-exports of the same module
     are the same binding. */

async function importErr(spec) {
    return import(spec).then(() => null, e => e);
}

/* a binding re-exported both ways is still one binding */
{
    const ns = await import("./fixture_export_hub_ok.js");
    assert(ns.foo, 2);
}

/* the same, checked against the two halves separately */
{
    const viaFrom = await import("./fixture_export_from.js");
    const viaImport = await import("./fixture_export_import_and_export.js");
    assert(viaFrom.foo, 2);
    assert(viaImport.foo, 2);
}

/* a module namespace re-exported twice is one binding, whichever form is
   used and in whichever combination */
{
    for (const spec of ["./fixture_export_hub_ns_ok.js",
                        "./fixture_export_hub_ns_import_ok.js",
                        "./fixture_export_hub_ns_mixed_ok.js"]) {
        const ns = await import(spec);
        assert(typeof ns.foo, "object", spec);
        assert(ns.foo[Symbol.toStringTag], "Module", spec);
    }

    /* and it is literally the same namespace object every time */
    const a = await import("./fixture_export_hub_ns_ok.js");
    const b = await import("./fixture_export_hub_ns_import_ok.js");
    const c = await import("./fixture_export_hub_ns_mixed_ok.js");
    const empty = await import("./fixture_export_empty.js");
    assert(a.foo === empty, true);
    assert(b.foo === empty, true);
    assert(c.foo === empty, true);
}

/* genuinely different bindings are still ambiguous. An ambiguous name is
   left out of the namespace object rather than throwing; importing it by
   name is what fails. */
{
    const ns = await import("./fixture_export_hub_ambiguous.js");
    assert("foo" in ns, false);
    assert(Object.keys(ns).indexOf("foo"), -1);

    const nsns = await import("./fixture_export_hub_ns_ambiguous.js");
    assert("foo" in nsns, false);

    /* the other names of the same modules are unaffected */
    const partial = await import("./fixture_export_hub_partial.js");
    assert("foo" in partial, false);
    assert(partial.second, 5);
    assert(partial.counter, 0);

    for (const spec of ["./fixture_export_hub_ambiguous_use.js",
                        "./fixture_export_hub_ns_ambiguous_use.js"]) {
        const e = await importErr(spec);
        assert(e instanceof SyntaxError, true, spec);
        assert(e.message.indexOf("ambiguous") >= 0, true, e.message);
    }
}

/* rewriting a re-exported import must not disturb the ordinary cases */
{
    const ns = await import("./fixture_export_renamed.js");

    /* an import renamed on the way in, then exported under the local name */
    assert(ns.f, 2);
    assert(ns.alsoF, 2);
    /* a plain local export alongside it */
    assert(ns.localOnly, 10);

    /* the export names are exactly the ones written, sorted */
    assert(Object.keys(ns).join(","), "alsoF,bump,counter,f,localOnly");

    /* the re-exported binding is live: mutating it in the defining module is
       visible through the re-export */
    const base = await import("./fixture_export_base.js");
    assert(ns.counter, 0);
    assert(base.counter, 0);
    ns.bump();
    assert(base.counter, 1);
    assert(ns.counter, 1);
    base.bump();
    assert(ns.counter, 2);
}

/* namespace objects are read-only views, however the name got there */
{
    const ns = await import("./fixture_export_hub_ok.js");
    assert(Reflect.set(ns, "foo", 99), false);
    assert(ns.foo, 2);
    assert(Reflect.defineProperty(ns, "bar", { value: 1 }), false);
    assert(Object.getOwnPropertyDescriptor(ns, "foo").writable, true);
    assert(Object.getOwnPropertyDescriptor(ns, "foo").configurable, false);
    assert("nope" in ns, false);
}

/* re-importing resolves to the same module instance each time */
{
    const a = await import("./fixture_export_import_and_export.js");
    const b = await import("./fixture_export_import_and_export.js");
    assert(a === b, true);
}
