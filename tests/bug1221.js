import {assert} from "./assert.js"

let caught = 0
for (let i = 1; i <= 32; i++) {
    const prefix = "(:?".repeat(i)
    const suffix = "|)+".repeat(i)
    const between = "(?:a|)+"
    try {
        new RegExp(prefix + between + suffix)
    } catch (e) {
        assert(e instanceof SyntaxError)
        assert(e.message, "out of memory")
        caught++
    }
}
assert(caught, 12) // subject to change
