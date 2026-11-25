import {assert} from "./assert.js"

// A generator object is created after the prologue of its function has run.
// Variable references created by the prologue (the mapped arguments object,
// closures in parameter initialisers) did not keep the generator alive: when
// it was only reachable through its own frame, the cycle collector freed it
// while those references still pointed into the frame.

// mapped arguments object, generator stored in its own parameter.
// The Function constructors make sloppy mode functions: this file is a
// module, and strict mode functions have no mapped arguments.
for (const ctor of [function*() {}.constructor, async function*() {}.constructor]) {
    let args
    await (async function() {
        const g = ctor("leak", "self", "leak(arguments); self = yield 1; yield 2;")
        const it = g((a) => { args = a }, 0)
        await it.next()   // an async generator resumes asynchronously
        await it.next(it) // self = it: only the frame holds the generator now
    })()
    gc()
    const it = args[1]
    assert(typeof it, "object")
    assert(typeof it.next, "function")
}

// closure over `this` in a parameter initialiser
for (const ctor of [function*() {}.constructor, async function*() {}.constructor]) {
    let leaked, ref
    ;(function() {
        const gen = ctor("leak", "a = leak(() => this)", "yield 1;")
        const o = { tag: "this" }
        ref = new WeakRef(o)
        o.g = gen.call(o, (f) => { leaked = f })
    })()
    gc()
    assert(ref.deref() !== undefined, true)
    assert(leaked().tag, "this")
    assert(typeof leaked().g.next, "function")
}

// closure over `arguments` in a parameter initialiser
{
    let leaked, ref
    ;(function() {
        function* gen(a = (leaked = () => arguments)) {}
        const g = gen(undefined, 2)
        const args = leaked()
        ref = new WeakRef(args)
        args.g = g
    })()
    gc()
    assert(ref.deref() !== undefined, true)
    assert(leaked().length, 2)
}

// nothing is kept alive once the references are dropped
{
    let leaked, ref
    ;(function() {
        function* gen(a = (leaked = () => this)) {}
        const o = {}
        ref = new WeakRef(o)
        o.g = gen.call(o)
    })()
    leaked = null
    gc()
    assert(ref.deref(), undefined)
}
