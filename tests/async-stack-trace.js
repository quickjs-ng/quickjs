import {assert} from "../tests/assert.js"

let ok = false
globalThis.exit = function() { assert(ok) }

f().catch(e => {
    assert(e instanceof Error)
    assert(typeof e.stack, "string")
    assert(e.message, "boom")
    assert(e.stack.includes(" at f "))
    ok = true
})

async function f() {
    await g()
}

async function g() {
    await 42
    throw Error("boom")
}
