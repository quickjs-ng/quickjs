import {assert} from "./assert.js"

// Substrings longer than 1024 bytes are slices that point into their parent
// string. Interning one as an atom (as a property key or a symbol
// description) used to keep the slice as it was: the atom's characters were
// not followed by a null byte, and the parent was never released.

{
    const parent = "0".repeat(1999) + "1" + "7".repeat(1000)
    const slice = parent.substring(0, 2000)
    const holder = {}
    holder[slice] = 1                      // string atom
    const desc = Symbol(slice).description // symbol atom, a copy
    assert(desc.length, 2000)
    assert(desc === slice, true)
    // these parse the characters as a C string
    assert(parseFloat(desc), 1)
    assert(parseInt(desc, 10), 1)
    assert(Number(desc), 1)
    assert(BigInt(desc), 1n)
    assert(Object.keys(holder)[0] === slice, true)
    assert(parseFloat(Object.keys(holder)[0]), 1)
}

{
    const parent = "k".repeat(1500) + "0".repeat(1100) + "5" + "9".repeat(900)
    const slice = parent.substring(1500, 2601)
    const sym = Symbol.for(slice)
    assert(Symbol.keyFor(sym) === slice, true)
    assert(parseFloat(sym.description), 5)
    const o = { [slice]: 1 }
    assert(o[parent.substring(1500, 2601)], 1)
    assert(parseFloat(Object.keys(o)[0]), 5)
}

// 16 bit strings
{
    const wide = ("б".repeat(1999) + "1" + "7".repeat(1000)).substring(0, 2000)
    const o = { [wide]: 1 }
    assert(Symbol(wide).description === wide, true)
    assert(Object.keys(o)[0].length, 2000)
    assert(o[wide], 1)
}
