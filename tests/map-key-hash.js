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
