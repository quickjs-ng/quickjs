import {assert} from "./assert.js"

// `new super()` is not valid syntax: super() is a call, not something `new`
// can be applied to. It used to compile to bytecode with one value too few on
// the stack.

function isSyntaxError(src) {
    try {
        (0, eval)(src)
    } catch (e) {
        return e instanceof SyntaxError
    }
    return false
}

for (const body of [
    "new super();",
    "super(); new super(1, 2);",
    "super(); new super;",
    "(() => new super())();",
    "super(); try { throw 1; } finally { switch (Object) { case new super(): } }",
]) {
    assert(isSyntaxError(`class A extends Object { constructor() { ${body} } }`), true, body)
}
assert(isSyntaxError("class A extends Object { constructor(a = new super()) {} }"), true)

// `new` applied to a super property is fine
class K {
    constructor(v) { this.v = v }
}
class B {}
B.prototype.K = K
class C extends B {
    constructor() {
        super()
        this.a = new super.K(1)
        this.b = new super["K"](2)
        this.c = new super.K
    }
}
const c = new C()
assert(c.a.v, 1)
assert(c.b.v, 2)
assert(c.c instanceof K, true)
