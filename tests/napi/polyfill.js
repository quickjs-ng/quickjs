globalThis.ext = os.platform === "win32" ? "dll" : "so"

globalThis.gc = std.gc
globalThis.global = globalThis

globalThis.assert = function(ok) {
    if (!ok)
        throw new Error("assertion failed")
}

globalThis.assert.ok = function(ok, message) {
    if (!ok)
        throw new Error(message)
}

globalThis.assert.strictEqual = function(actual, expected) {
    if (actual !== expected)
        throw new Error(`${actual} != ${expected}`)
}

globalThis.assert.throws = function(f, expected) {
    let ok = false
    try {
        f()
    } catch (e) {
        if (!RegExp(expected).test(e.message))
            throw new Error(`unexpected error: ${e.message}`)
        ok = true
    }
    if (!ok)
        throw new Error("missing exception")
}

globalThis.common = {
    expectsError(f, expected) {
        throw new Error("todo")
    },
    mustCall(f) {
        throw new Error("todo")
    },
    mustNotCall() {
        throw new Error("todo")
    },
}
