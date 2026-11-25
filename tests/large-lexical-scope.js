import {assert, assertThrows} from "./assert.js"

const vars = Array.from({length: 32}, (_, i) => `let v${i} = ${i};`).join("\n")
const fn = body => Function(vars + "\n" + body)

assert(fn("{ let v31 = 42; if (v31 !== 42) return -1; } return v31;")(), 31)
assert(fn("function f(x = v31) { let v31 = 42; return x; } return f();")(), 31)
assert(fn("with ({v31: 42}) { return (function() { return v31; })(); }")(), 42)
assert(fn("try { throw 42; } catch (v31) { return v31; }")(), 42)
assertThrows(TypeError, fn("const c = 1; c = 2;"))
assertThrows(SyntaxError, () => fn("let v31;"))
assertThrows(SyntaxError, () => fn("{ var v31; }"))

const values = Array.from({length: 32}, (_, i) => i)
const blocks = values.map(i => `{ let x = ${i}; out.push(x); }`).join("\n")
assert(fn("const out = [];\n" + blocks + "\nreturn out;")(), values)
assert(fn("{".repeat(64) + "return v31;" + "}".repeat(64))(), 31)
