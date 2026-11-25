import {assert} from "./assert.js"

// `for (await using x of y)` needs an async function, also where `await` is
// a keyword for other reasons: class static blocks, and every function of a
// module.

function compiles(src) {
    try {
        (0, eval)(src)
    } catch (e) {
        assert(e instanceof SyntaxError, true, src)
        return false
    }
    return true
}

assert(compiles("class C { static { for (await using x of []) {} } }"), false)
assert(compiles("class C { static { const f = () => { for (await using x of []) {} }; } }"), false)
assert(compiles("(async function() { for (await using x of []) {} })"), true)
assert(compiles("(async () => { for (await using x of []) {} })"), true)
assert(compiles("(async function*() { for (await using x of []) {} })"), true)
assert(compiles("class C { static { (async () => { for (await using x of []) {} }); } }"), true)

// a plain function of a module
let error
try {
    await import("./fixture_for_await_using_sync.js")
} catch (e) {
    error = e
}
assert(error instanceof SyntaxError, true)

// still valid at the top level of a module, and it disposes
const log = []
for (await using x of [{ async [Symbol.asyncDispose]() { log.push("disposed") } }]) {
    log.push("body")
}
assert(log.join(), "body,disposed")
