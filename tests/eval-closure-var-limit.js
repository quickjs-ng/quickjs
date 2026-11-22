import {assert} from "./assert.js"

// A direct eval sees the parameters, the local variables and the closure
// variables of the function it is called from, and gets one closure variable
// for each. Their number is stored on 16 bits and was not checked.

const n = 32768
const inner = Function("a,".repeat(n - 1) + "a",
    "return function(" + "a,".repeat(1000) + "z," + "a,".repeat(n - 1002) + "a" +
    ") { return eval('z'); }")()
let threw
try {
    inner()
} catch (e) {
    threw = e
}
assert(threw instanceof SyntaxError, true)

assert((function(z) { return eval("z") })(42), 42)
