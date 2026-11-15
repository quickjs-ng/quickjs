import {assert} from "./assert.js"

// A function can hold 65535 local variables and 65535 parameters. When the
// compiler needed one more slot while resolving variables (for `arguments`,
// `this`, the function's own name, ...), the failure was ignored: the name was
// compiled as a global lookup, or a bogus index was emitted. Compilation must
// fail with a SyntaxError instead.

const names = []
for (let i = 0; i < 65535; i++)
    names.push("_" + i.toString(36))

function compile(count, head, tail) {
    try {
        return (0, eval)(`${head} var ${names.slice(0, count).join(",")}; ${tail}`)
    } catch (e) {
        return e
    }
}

for (const [head, tail] of [
    ["(function f() {", "return arguments; })"],
    ["(function f() {", "return () => this; })"],
    ["(function f() {", "return f; })"],
]) {
    // no slot left for the implicit binding
    assert(compile(65535, head, tail) instanceof SyntaxError, true, tail)
    // one slot left
    assert(typeof compile(65534, head, tail), "function", tail)
}
assert(compile(3, "(function f() {", "return [arguments.length, this, typeof f]; })")
       .call(7, 1, 2).join(), "2,7,function")

// A destructuring parameter takes an unnamed argument slot as well.
function compileParams(count) {
    try {
        return (0, eval)(`(function(${"[],".repeat(count)}[x]) {})`)
    } catch (e) {
        return e
    }
}
assert(compileParams(65535) instanceof SyntaxError, true)
assert(typeof compileParams(65534), "function")
