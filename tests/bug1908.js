import {assert} from "./assert.js"

// A direct eval in a parameter list that binds no name: the argument scope is
// empty, and its end marker was mistaken for a variable index.

var outer = 42, seen

var f = ({} = eval("1")) => 1
assert(f(), 1)
assert(f({}), 1)

class C { m([] = eval("[]")) { return 2 } }
assert(new C().m(), 2)

function g({} = eval("1")) { return 3 }
assert(g(), 3)

var h = (...[{} = eval("1")]) => 4
assert(h(), 4)

var k = ({} = eval(...["1"])) => 5
assert(k(), 5)

// the eval still sees the enclosing scopes, `this` and new.target
var r = ({} = eval("seen = outer + 1")) => seen
assert(r(), 43)
function t({} = eval("seen = this.v + new.target.length")) {}
t.prototype.v = 10
new t()
assert(seen, 10)

// a named parameter in the same list is visible
var n = (a, {} = eval("a")) => a
assert(n(6), 6)

// An eval whose scope starts at the last variable slot cannot be encoded.
{
    const names = []
    for (let i = 0; i < 65534; i++)
        names.push("_" + i.toString(36))
    const compile = (count) => {
        try {
            return typeof (0, eval)(`(() => { 'use strict'; var ${names.slice(0, count).join(",")}; { let z = 7; return eval('z'); } })`)
        } catch (e) {
            return e
        }
    }
    assert(compile(65533), "function")
    assert(compile(65534) instanceof SyntaxError, true)
}
