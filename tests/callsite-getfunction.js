import {assert} from "./assert.js"

// CallSite.prototype.getFunction() must not hand out functions that script
// cannot get any other way and is not meant to call.

function callSites() {
    const saved = Error.prepareStackTrace
    Error.prepareStackTrace = (e, sites) => sites
    const sites = new Error().stack
    Error.prepareStackTrace = saved
    return sites.slice(1) // drop this helper's own frame
}

// The body of a module is not an ordinary function.
{
    const sites = callSites()
    assert(sites[0].isNative(), false)
    assert(sites[0].getFunction(), undefined)
}

// Ordinary functions and builtins are still reported.
{
    function f() { return callSites() }
    assert(f()[0].getFunction(), f)
    const sites = [1].map(function m() { return callSites() })[0]
    assert(typeof sites[0].getFunction(), "function")
    const native = sites.find((s) => s.isNative())
    assert(native.getFunction(), Array.prototype.map)
}

// The engine's own closures: the frame that resumes an async generator
// after an await, and the one that settles a promise reaction.
{
    const seen = []
    function record() {
        for (const s of callSites())
            if (s.isNative())
                seen.push(s.getFunction())
    }
    async function* gen() {
        await 0
        record()
        try {
            await Promise.reject(1)
        } catch {
            record()
        }
        yield 3
    }
    const r = await gen().next()
    assert(r.value, 3)
    assert(seen.length > 0, true)
    for (const f of seen)
        assert(f, undefined)
}

{
    const seen = []
    await Promise.resolve().then(() => {
        for (const s of callSites())
            if (s.isNative())
                seen.push(s.getFunction())
    })
    for (const f of seen)
        assert(f, undefined)
}
