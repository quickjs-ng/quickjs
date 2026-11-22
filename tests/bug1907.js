import {assert} from "./assert.js"

// Map/Set clear(): deleting a record can free the record after it, when that
// one was already deleted and is only kept alive by an iterator that the
// record being deleted holds.

{
    const m = new Map()
    m.set("a", 0)
    m.set("b", 0)
    let it = m.keys()
    it.next()
    it.next()      // the iterator pins record "b"
    m.delete("b")  // "b" is now kept only for the iterator
    m.set("a", it) // record "a" holds the only reference to the iterator
    it = null
    m.clear()
    assert(m.size, 0)
}

{
    const s = new Set()
    let h = {}
    s.add(h)
    s.add("b")
    let it = s.values()
    it.next()
    it.next()
    s.delete("b")
    h.it = it
    h = it = null
    s.clear()
    assert(s.size, 0)
}

{
    // several deleted records in a row, all pinned by one iterator
    const m = new Map([["a", 0], ["b", 1], ["c", 2], ["d", 3]])
    let it = m.entries()
    it.next()
    it.next()
    m.delete("b")
    m.delete("c")
    m.set("a", it)
    it = null
    m.clear()
    assert(m.size, 0)
}

{
    // an iterator that survives clear() ends cleanly
    const m = new Map([["a", 1], ["b", 2]])
    const it = m.keys()
    assert(it.next().value, "a")
    m.clear()
    assert(it.next().done, true)
    m.set("c", 3)
    assert(m.size, 1)
}
