import {assert} from "./assert.js"

// Keys that are the same value must have the same hash in Map and Set,
// however they are represented inside the engine.

// an int32 and a float64 that are equal
;(function() {
    const f64 = new Float64Array(1)
    const m = new Map
    for (let i = -300; i < 300; i++)
        m.set(i, i)
    for (let i = -300; i < 300; i++) {
        f64[0] = i
        assert(m.get(f64[0]), i)
        assert(m.get(i + 0.5 - 0.5), i)
        assert(m.has(i + 0.5), false)
    }
    assert(m.size, 600)
    const s = new Set
    for (let i = -300; i < 300; i++) {
        f64[0] = i
        s.add(f64[0])
    }
    for (let i = -300; i < 300; i++)
        assert(s.has(i), true)
})()

// both zeros, and every NaN
;(function() {
    const f64 = new Float64Array(1)
    const u32 = new Uint32Array(f64.buffer)
    const m = new Map
    m.set(-0, "zero")
    assert(m.get(0), "zero")
    assert(Object.is(m.keys().next().value, 0), true)
    m.set(NaN, "nan")
    for (const bits of [0x7FF80000, 0xFFF80000, 0x7FF00000, 0xFFFFFFFF]) {
        u32[0] = 1
        u32[1] = bits
        assert(Number.isNaN(f64[0]), true)
        assert(m.get(f64[0]), "nan")
    }
    assert(m.get(0 / 0), "nan")
    assert(m.size, 2)
})()

// bigints around the limits of the representations, computed in two ways
;(function() {
    const m = new Map
    const keys = []
    for (const k of [0n, 31n, 32n, 63n, 64n, 127n, 128n]) {
        for (const d of [-1n, 0n, 1n]) {
            keys.push(2n ** k + d)
            keys.push(-(2n ** k) + d)
        }
    }
    const s = new Set(keys)
    for (const k of s)
        m.set(k, String(k))
    for (const k of s) {
        assert(m.get(BigInt(String(k))), String(k))
        assert(m.get(k * 3n / 3n), String(k))
    }
    assert(m.size, s.size)
    // not the number of the same value
    m.clear()
    for (let i = -10; i < 10; i++)
        m.set(i, "number")
    for (let i = -10n; i < 10n; i++) {
        assert(m.has(i), false)
        m.set(i, "bigint")
    }
    assert(m.size, 40)
    assert(m.get(1), "number")
    assert(m.get(1n), "bigint")
})()

// keys of different types that look alike
;(function() {
    const keys = [0, 1, false, true, null, undefined, "0", "1", "false",
                  "true", "null", "undefined", "", 0n, 1n]
    const m = new Map
    keys.forEach((k, i) => m.set(k, i))
    assert(m.size, keys.length)
    keys.forEach((k, i) => assert(m.get(k), i))
})()

// objects and symbols, enough of them to grow the table a number of times
;(function() {
    const objs = [], syms = []
    const s = new Set
    for (let i = 0; i < 5000; i++) {
        objs.push({})
        syms.push(Symbol())
        s.add(objs[i])
        s.add(syms[i])
    }
    assert(s.size, 10000)
    for (let i = 0; i < 5000; i++) {
        assert(s.has(objs[i]), true)
        assert(s.has(syms[i]), true)
        assert(s.has({}), false)
        assert(s.has(Symbol()), false)
    }
    for (let i = 0; i < 5000; i += 2)
        assert(s.delete(objs[i]), true)
    for (let i = 0; i < 5000; i++)
        assert(s.has(objs[i]), i % 2 === 1)
    assert(s.size, 7500)
})()

// a rope, which is what the concatenation of long strings gives, and the
// string that it stands for
;(function() {
    const flat = (s) => JSON.parse(JSON.stringify(s))
    const a = "x".repeat(5000), b = "y".repeat(5001)
    const c = "\u4E00".repeat(3000), d = "\xE9".repeat(3003)
    // every line is one string, put together in different ways
    const strings = [
        [() => a + b,
         () => a + b.slice(0, 2500) + b.slice(2500),
         () => a.slice(0, 1234) + (a.slice(1234) + b)],
        [() => a + c, () => a.slice(0, 2501) + (a.slice(2501) + c)],
        [() => c + a + c, () => c + (a + c)],
        [() => d + a,
         () => d.slice(0, 1500) + d.slice(1500) + a,
         () => (c + d).slice(3000) + a],
        [() => a + b + c + d + a + b + c + d,
         () => (a + b + c + d) + (a + b + c + d)],
    ]
    const s = new Set, m = new Map
    for (let i = 0; i < 1000; i++) {
        s.add("filler" + i)
        m.set("filler" + i, i)
    }
    strings.forEach((ropes, i) => {
        const str = flat(ropes[0]())
        // rope in the table
        s.add(ropes[0]())
        assert(s.size, 1001 + i)
        // string in the table
        m.set(str, i)
        assert(m.size, 1001 + i)
        for (const rope of ropes) {
            assert(rope() === str, true)
            assert(s.has(rope()), true)
            assert(s.has(str), true)
            assert(s.has(rope() + "!"), false)
            s.add(str)
            s.add(rope())
            assert(s.size, 1001 + i)
            assert(m.get(rope()), i)
            m.set(rope(), i)
            assert(m.size, 1001 + i)
        }
        assert(m.delete(ropes[0]()), true)
        assert(m.has(str), false)
        m.set(ropes[0](), i)
        assert(m.get(str), i)
    })
})()

// A record keeps the hash of its key. The table uses it when it grows,
// and Set methods use it when they copy records to a new Set.
;(function() {
    const obj = {}, sym = Symbol()
    const rope = "x".repeat(3000) + "y".repeat(3000)
    const keys = [0, 1, -1, 0.5, NaN, 2 ** 40, "", "a", "一", rope,
                  10n, 1n << 70n, true, null, undefined, obj, sym]
    const check = (t) => {
        for (const k of keys)
            assert(t.has(k), true)
    }
    const s = new Set(keys)
    // grows the table several times
    for (let i = 0; i < 5000; i++)
        s.add("filler" + i)
    assert(s.size, 5000 + keys.length)
    check(s)
    const fillers = new Set
    for (let i = 0; i < 6000; i++)
        fillers.add("filler" + i)
    const small = new Set(keys)
    const sets = [
        [s.union(new Set(["a", obj, "new"])), s.size + 1],
        [s.symmetricDifference(new Set(["new"])), s.size + 1],
        [s.intersection(small), keys.length],
        [small.intersection(s), keys.length],
        [s.difference(new Set(["filler0"])), s.size - 1],
        [small.difference(fillers), keys.length],
    ]
    for (const [t, size] of sets) {
        assert(t.size, size)
        check(t)
        // the copies are found again after the new table has grown
        for (let i = 0; i < 20000; i++)
            t.add(i + 0.25)
        assert(t.size, size + 20000)
        check(t)
    }
    assert(s.difference(fillers).size, keys.length)
    assert(small.symmetricDifference(small).size, 0)
    // an iterator holds on to a deleted record while the table grows
    const m = new Map([[1, "a"], [2, "b"]])
    const it = m.keys()
    assert(it.next().value, 1)
    m.delete(1)
    for (let i = 10; i < 1000; i++)
        m.set(i, i)
    assert(m.has(1), false)
    assert(it.next().value, 2)
    m.set(1, "c")
    assert(m.get(1), "c")
    // the callback grows the table between the lookup and the insertion
    const g = new Map
    const v = g.getOrInsertComputed("k", () => {
        for (let i = 0; i < 1000; i++)
            g.set(i, i)
        return 42
    })
    assert(v, 42)
    assert(g.get("k"), 42)
    assert(g.size, 1001)
})()

// records leave their bucket one by one, in any order, also when the
// garbage collector removes them from a WeakMap
;(function() {
    const m = new Map
    for (let i = 0; i < 3000; i++)
        m.set("k" + i, i)
    for (let i = 0; i < 3000; i += 3)
        assert(m.delete("k" + i), true)
    assert(m.size, 2000)
    for (let i = 0; i < 3000; i++)
        assert(m.get("k" + i), i % 3 ? i : undefined)
    for (let i = 2999; i >= 0; i -= 3)
        assert(m.delete("k" + i), true)
    for (let i = 0; i < 3000; i += 3)
        m.set("k" + i, -i)
    assert(m.size, 2000)
    for (let i = 0; i < 3000; i++)
        assert(m.get("k" + i), [-i, i, undefined][i % 3])
    const wm = new WeakMap
    let keys = []
    for (let i = 0; i < 3000; i++) {
        keys.push({})
        wm.set(keys[i], i)
    }
    const kept = keys.filter((k, i) => i % 3 === 0)
    keys = null
    gc()
    kept.forEach((k, i) => assert(wm.get(k), 3 * i))
    for (let i = 0; i < 1000; i += 2)
        assert(wm.delete(kept[i]), true)
    kept.forEach((k, i) => assert(wm.has(k), i % 2 === 1))
})()

// A string has the same hash whether or not it is an atom, also when it
// becomes one while it is a key.
;(function() {
    const fresh = (i) => ["at", "om", i].join("") // not an atom
    const m = new Map, s = new Set
    for (let i = 0; i < 500; i++) {
        m.set(fresh(i), i)
        s.add(fresh(i))
    }
    // property names are atoms: every other key of |m| becomes one
    const o = {}
    for (const k of m.keys()) {
        if (m.get(k) % 2)
            o[k] = true
    }
    const names = Object.keys(o)
    assert(names.length, 250)
    for (let i = 0; i < 500; i++) {
        assert(m.get(fresh(i)), i)
        assert(s.has(fresh(i)), true)
    }
    for (const k of names) {
        assert(m.get(k) % 2, 1)
        assert(s.has(k), true)
    }
    assert(m.get("atom7"), 7)
    assert(m.get("atom8"), 8)
    assert(s.has("atom499"), true)
    assert(s.has("atom500"), false)
    // atoms in the table, other strings to look them up
    const t = new Set(names)
    t.add("literal")
    for (let i = 1; i < 500; i += 2)
        assert(t.has(fresh(i)), true)
    assert(t.has(fresh(2)), false)
    assert(t.has(["lite", "ral"].join("")), true)
    assert(t.size, 251)
    // 16 bit strings and ropes
    const wide = {"一tom": 1, "\xE9tom": 2}
    const long = {["x".repeat(3000) + "y".repeat(3000)]: 3}
    const u = new Map
    for (const k of [...Object.keys(wide), ...Object.keys(long)])
        u.set(k, k.length)
    assert(u.get(["一", "tom"].join("")), 4)
    assert(u.get(("一" + "\xE9tom").slice(1)), 4)
    assert(u.get("x".repeat(3000) + "y".repeat(3000)), 6000)
    assert(u.size, 3)
})()
