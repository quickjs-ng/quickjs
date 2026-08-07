import { assert } from "./assert.js";

/* ==, !=, === and !== over every combination that matters. The interpreter
   handles the common shapes inline and hands everything else to the slow
   path, so the two have to agree everywhere. */

function check(a, b, loose, strict, what) {
    assert(a == b, loose, `${what}: ==`);
    assert(a != b, !loose, `${what}: !=`);
    assert(a === b, strict, `${what}: ===`);
    assert(a !== b, !strict, `${what}: !==`);
    /* the operators are symmetric */
    assert(b == a, loose, `${what}: == reversed`);
    assert(b != a, !loose, `${what}: != reversed`);
    assert(b === a, strict, `${what}: === reversed`);
    assert(b !== a, !strict, `${what}: !== reversed`);
}

/* object identity: the only thing that makes two objects equal is being the
   same object */
{
    const o = {};
    check(o, o, true, true, "same object");
    check({}, {}, false, false, "two objects");
    check([], [], false, false, "two arrays");

    const a = [1, 2];
    check(a, a, true, true, "same array");

    const f = function() {};
    check(f, f, true, true, "same function");
    check(function() {}, function() {}, false, false, "two functions");

    const p = new Proxy(o, {});
    check(p, p, true, true, "same proxy");
    check(p, o, false, false, "proxy vs target");

    check(Object(1), Object(1), false, false, "two boxed numbers");
    const n = Object(1);
    check(n, n, true, true, "same boxed number");

    check(Math, Math, true, true, "namespace object");
    check(globalThis, globalThis, true, true, "global object");

    /* an object is never equal to a plain object literal of the same shape */
    check({ a: 1 }, { a: 1 }, false, false, "structurally equal objects");
}

/* strings compare by value, however they were built */
{
    check("abc", "abc", true, true, "identical literals");
    check("abc", "abd", false, false, "differing literals");
    check("abc", "ab", false, false, "differing lengths");
    check("", "", true, true, "empty strings");
    check("a", "", false, false, "empty vs non-empty");

    check("abc", "ab" + "c", true, true, "literal vs concatenation");
    check("abc", ["a", "b", "c"].join(""), true, true, "literal vs join");
    check("abc", "xabcx".slice(1, 4), true, true, "literal vs slice");
    check("abc", String("abc"), true, true, "literal vs String()");
    check("aaa", "a".repeat(3), true, true, "literal vs repeat");

    /* wide characters: the two operands can have different internal widths */
    check("é", "é", true, true, "latin1");
    check("中文", "中" + "文", true, true, "wide");
    check("a中", "a" + "中", true, true, "mixed width");
    check("a", "a", true, true, "escape vs literal");
    check("\0", "\0", true, true, "NUL");
    check("a\0b", "a\0b", true, true, "embedded NUL");
    check("a\0b", "a\0c", false, false, "embedded NUL differs");

    /* long strings, built two different ways, land in the same place */
    {
        let x = "";
        for (let i = 0; i < 200; i++)
            x += "abcdefgh";
        let y = "";
        for (let i = 0; i < 200; i++)
            y += "abcdefgh";
        check(x, y, true, true, "long concatenated strings");
        assert(x.length, 1600);

        const z = x + "!";
        check(x, z, false, false, "long strings differing in the tail");
        check(x, "abcdefgh".repeat(200), true, true, "concat vs repeat");
    }
}

/* a string and a String object are loosely but not strictly equal */
{
    check("abc", new String("abc"), true, false, "string vs String object");
    check(new String("abc"), new String("abc"), false, false, "two String objects");
    const s = new String("abc");
    check(s, s, true, true, "same String object");
}

/* numbers */
{
    check(1, 1, true, true, "same int");
    check(1, 2, false, false, "different ints");
    check(1, 1.0, true, true, "int vs double");
    check(0.1 + 0.2, 0.30000000000000004, true, true, "double");
    check(0, -0, true, true, "zero and negative zero");
    check(Infinity, Infinity, true, true, "infinity");
    check(Infinity, -Infinity, false, false, "opposite infinities");

    /* NaN is equal to nothing, itself included */
    check(NaN, NaN, false, false, "NaN");
    assert(NaN == NaN, false);
    assert(NaN != NaN, true);
    assert(NaN === NaN, false);
    assert(NaN !== NaN, true);

    check(1, "1", true, false, "number vs numeric string");
    check(1, "1.0", true, false, "number vs decimal string");
    check(0, "", true, false, "zero vs empty string");
    check(0, "0", true, false, "zero vs zero string");
    check(NaN, "NaN", false, false, "NaN vs its string");
    check(1, true, true, false, "one vs true");
    check(0, false, true, false, "zero vs false");
    check(1, [1], true, false, "number vs single element array");
    check(0, [], true, false, "zero vs empty array");
}

/* null and undefined are loosely equal to each other and nothing else */
{
    check(null, null, true, true, "null");
    check(undefined, undefined, true, true, "undefined");
    check(null, undefined, true, false, "null vs undefined");
    check(null, 0, false, false, "null vs zero");
    check(null, false, false, false, "null vs false");
    check(null, "", false, false, "null vs empty string");
    check(undefined, 0, false, false, "undefined vs zero");
    check(null, {}, false, false, "null vs object");
    check(undefined, {}, false, false, "undefined vs object");
}

/* booleans */
{
    check(true, true, true, true, "true");
    check(true, false, false, false, "true vs false");
    check(true, "1", true, false, "true vs '1'");
    check(false, "0", true, false, "false vs '0'");
    check(true, {}, false, false, "true vs object");
}

/* bigints */
{
    check(1n, 1n, true, true, "same bigint");
    check(1n, 2n, false, false, "different bigints");
    check(1n, 1, true, false, "bigint vs number");
    check(1n, 1.5, false, false, "bigint vs non-integral number");
    check(1n, "1", true, false, "bigint vs string");
    check(1n, true, true, false, "bigint vs true");
    check(1n, NaN, false, false, "bigint vs NaN");
    check(0n, -0, true, false, "zero bigint vs negative zero");
    check((1n << 70n), (1n << 70n), true, true, "large bigints");
    check((1n << 70n), (1n << 70n) + 1n, false, false, "large bigints differ");
}

/* symbols are only equal to themselves */
{
    const s = Symbol("s");
    check(s, s, true, true, "same symbol");
    check(Symbol("s"), Symbol("s"), false, false, "two symbols");
    check(Symbol.iterator, Symbol.iterator, true, true, "well known symbol");
    check(s, Object(s), true, false, "symbol vs boxed symbol");
}

/* loose equality against an object coerces it, strict equality never does */
{
    let calls = 0;
    const o = { valueOf() { calls++; return 42; } };
    assert(o == 42, true);
    assert(calls, 1);
    assert(o === 42, false);
    assert(calls, 1);

    calls = 0;
    const t = { toString() { calls++; return "s"; } };
    assert(t == "s", true);
    assert(calls, 1);
    assert(t === "s", false);
    assert(calls, 1);

    /* Symbol.toPrimitive wins */
    const p = { [Symbol.toPrimitive]() { return 7; } };
    assert(p == 7, true);
    assert(p == "7", true);
    assert(p === 7, false);

    /* two objects are compared by identity, so nothing is coerced */
    calls = 0;
    const a = { valueOf() { calls++; return 1; } };
    const b = { valueOf() { calls++; return 1; } };
    assert(a == b, false);
    assert(calls, 0);
    assert(a == a, true);
    assert(calls, 0);

    /* and neither is anything under === */
    calls = 0;
    assert(a === b, false);
    assert(a === 1, false);
    assert(calls, 0);
}

/* a throwing coercion propagates out of == but is never reached by === */
{
    const boom = { valueOf() { throw new RangeError("boom"); } };
    let caught = null;
    try {
        boom == 1;
    } catch (e) {
        caught = e;
    }
    assert(caught instanceof RangeError, true);

    caught = null;
    try {
        assert(boom === 1, false);
        assert(boom != boom, false);
    } catch (e) {
        caught = e;
    }
    assert(caught, null);
}

/* the same values through variables the interpreter cannot fold away */
{
    const vals = [0, -0, 1, NaN, Infinity, "", "a", "1", true, false, null,
                  undefined, 1n, Symbol.iterator, {}, []];
    for (let i = 0; i < vals.length; i++) {
        for (let j = 0; j < vals.length; j++) {
            const a = vals[i], b = vals[j];
            /* === and !== are exact complements, always */
            assert((a === b) !== (a !== b), true, `${String(a)}/${String(b)}`);
            assert((a == b) !== (a != b), true, `${String(a)}/${String(b)}`);
            /* identity implies strict equality, except for NaN */
            if (i === j && !(typeof a === "number" && isNaN(a)))
                assert(a === b, true, `self ${String(a)}`);
            /* strict equality implies loose equality */
            if (a === b)
                assert(a == b, true, `strict implies loose ${String(a)}`);
        }
    }
}

/* Strings above the rope thresholds are stored as ropes rather than as flat
   character arrays, so an operand pair can be rope/rope, rope/flat or
   flat/flat for the same two sequences of characters. All three have to
   agree, at each threshold and on either side of it. */
{
    function grow(unit, n) {
        let s = "";
        for (let i = 0; i < n; i++)
            s += unit;
        return s;
    }

    for (const len of [8, 511, 512, 513, 8191, 8192, 8193, 20000]) {
        const rope = grow("a", len);
        const flat = "a".repeat(len);
        assert(rope.length, len);
        check(rope, flat, true, true, `rope vs flat ${len}`);
        check(rope, grow("a", len), true, true, `rope vs rope ${len}`);
        check(rope, flat + "b", false, false, `rope vs longer ${len}`);
        check(rope, "b" + flat.slice(1), false, false, `rope differing head ${len}`);
        check(rope, flat.slice(0, len - 1) + "b", false, false,
              `rope differing tail ${len}`);
    }

    /* a rope whose halves are of different widths, and a rope that flattens
       to the same characters through a different split */
    const wide = grow("中", 600);
    check(wide, "中".repeat(600), true, true, "wide rope vs flat");
    check("a".repeat(600) + "中", "a".repeat(600) + "中", true, true,
          "mixed width rope");
    const left = "x".repeat(700), right = "y".repeat(700);
    check(left + right, left + right, true, true, "rope halves");
    check(left + right, "x".repeat(699) + "y".repeat(701), false, false,
          "rope halves differing");

    /* a rope compared against the atom the same characters would intern to */
    const key = grow("k", 600);
    const obj = { [key]: 1 };
    check(Object.keys(obj)[0], key, true, true, "atom vs rope");
    assert(obj[grow("k", 600)], 1);
}

/* the interpreter reaches the inline comparison only from bytecode, and only
   after the operands have already been pushed, so run each shape enough
   times that a stale stack slot or a leaked reference would show up */
{
    const o1 = {}, o2 = {};
    const s1 = "hello world", s2 = "hello" + " " + "world", s3 = "hello worle";
    const long1 = "z".repeat(4000), long2 = "z".repeat(3999) + "z";
    let hits = 0;

    for (let i = 0; i < 2000; i++) {
        if (o1 == o1) hits++;
        if (o1 === o1) hits++;
        if (o1 != o2) hits++;
        if (o1 !== o2) hits++;
        if (s1 == s2) hits++;
        if (s1 === s2) hits++;
        if (s1 != s3) hits++;
        if (s1 !== s3) hits++;
        if (long1 == long2) hits++;
        if (long1 === long2) hits++;
        if (i == i) hits++;
        if (i === i) hits++;

        /* and the shapes that must never be true */
        if (o1 == o2) hits += 1000;
        if (o1 === o2) hits += 1000;
        if (s1 == s3) hits += 1000;
        if (s1 === s3) hits += 1000;
        if (i != i) hits += 1000;
    }
    assert(hits, 2000 * 12);
}
