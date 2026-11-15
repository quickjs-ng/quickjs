import {assert} from "./assert.js"

// The number of captured variables of a function is stored on 16 bits, but
// parameters and local variables are limited separately (65535 each), so
// together more than 65535 could be captured and the count wrapped around.

function compile(params, body) {
    try {
        return Function(params, body)
    } catch (e) {
        return e
    }
}

// a mapped arguments object captures every parameter
const params = "a,".repeat(65534) + "a" // 65535 parameters
assert(compile(params, "var x; (() => x); return arguments;") instanceof SyntaxError, true)
// exactly at the limit
assert(typeof compile(params, "return arguments;"), "function")

const f = Function("a", "var x; (() => x); arguments[0] = 7; return a;")
assert(f(1), 7)
