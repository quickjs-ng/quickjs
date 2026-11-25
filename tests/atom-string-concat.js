import {assert} from "./assert.js"

// A string that is used as a property name becomes an atom. When it was
// the left operand of a concatenation and had no other reference, the
// characters of the right operand were appended to it in place: the atom
// table then held a string under the hash of its old characters, and an
// equal string got a second atom.

function atom(a, b) {
    const s = [a, b].join("") // not an atom yet
    const o = {}
    o[s] = 1
    delete o[s]
    return s // the only reference
}

for (const [a, b, c] of [
    ["a", "b", "cd"],
    ["p", "q", "r"],
    ["abcdefgh", "ijk", "l"],
    ["key", "name", "x"],
    ["一", "丁", "丂"],
    ["leng", "t", "h"],
]) {
    const t = atom(a, b) + c
    const other = [a, b + c].join("")
    assert(t === other, true)
    const o = {}
    o[t] = 1
    assert(o[other], 1)
    assert(other in o, true)
    assert(Object.keys(o).length, 1)
    o[other] = 2
    assert(Object.keys(o).length, 1)
    assert(o[t], 2)
    // the atom of the left operand is intact
    const p = {[atom(a, b)]: 3}
    assert(p[[a, b].join("")], 3)
    const m = new Map([[t, 1]])
    assert(m.get(other), 1)
}

// "length" must be the one and only atom of that name
{
    const t = atom("leng", "t") + "h"
    assert([1, 2, 3][t], 3)
    assert("abc"[t], 3)
}
