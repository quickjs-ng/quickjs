"use strict"
const expect = "function f() { return 42 }"
function f() { return 42 }

{
    const actual = f.toString()
    if (actual !== expect) throw Error(actual)
}

{
    const f = eval(expect + "f")
    const actual = f.toString()
    if (actual !== expect) throw Error(actual)
}
