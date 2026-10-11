import { assert } from "./assert.js";

/* CanonicalNumericIndexString("NaN") is NaN, not undefined: ToString(NaN) is
   "NaN". On an integer-indexed exotic object that makes "NaN" an (always out
   of range) numeric index rather than an ordinary property, so it never
   reaches the ordinary object machinery or the prototype chain. */

const ta = new Int32Array(4);

/* the strings that are canonical numeric index strings but never a valid
   index, so every one of them is handled by the integer-indexed path */
const canonical = ["NaN", "Infinity", "-Infinity", "-0", "1.5", "-1", "4",
                   "1e+21"];
/* ... and lookalikes that are not canonical: ToString(ToNumber(s)) !== s */
const ordinary = ["-NaN", "nan", "NAN", "Nan", "NaN ", " NaN", "+NaN", "NaNa",
                  "aNaN", "NaN\0", "+Infinity", "0x1", "01", "1.50", "1e21",
                  ""];

/* [[Get]] returns undefined and does not consult the prototype */
{
    const proto = {};
    for (const k of canonical.concat(ordinary))
        proto[k] = "from proto";
    const t = new Int32Array(4);
    Object.setPrototypeOf(t, proto);

    for (const k of canonical)
        assert(t[k], undefined, `get ${k}`);
    for (const k of ordinary)
        assert(t[k], "from proto", `get ${k}`);

    /* the number NaN as a key goes through the same string */
    assert(t[NaN], undefined);
    assert(t[0 / 0], undefined);
}

/* [[HasProperty]] is false and does not consult the prototype */
{
    const proto = {};
    for (const k of canonical.concat(ordinary))
        proto[k] = 1;
    const t = new Int32Array(4);
    Object.setPrototypeOf(t, proto);

    for (const k of canonical)
        assert(k in t, false, `in ${k}`);
    for (const k of ordinary)
        assert(k in t, true, `in ${k}`);
    assert(NaN in t, false);
}

/* [[Set]] is silently dropped, and no ordinary property appears */
{
    const t = new Int32Array(4);
    for (const k of canonical) {
        t[k] = 123;
        assert(t[k], undefined, `set ${k}`);
        assert(Object.prototype.hasOwnProperty.call(t, k), false, `own ${k}`);
        assert(Reflect.set(t, k, 123), true, `Reflect.set ${k}`);
        assert(t[k], undefined, `after Reflect.set ${k}`);
    }
    for (const k of ordinary) {
        t[k] = 123;
        assert(t[k], 123, `set ${k}`);
        assert(Object.prototype.hasOwnProperty.call(t, k), true, `own ${k}`);
    }
    assert(Object.keys(t).indexOf("NaN"), -1);

    /* strict mode does not turn the dropped write into a TypeError */
    (function() {
        "use strict";
        const s = new Int32Array(4);
        s.NaN = 1;
        s[NaN] = 2;
        assert(s.NaN, undefined);
    })();
}

/* [[GetOwnProperty]] reports nothing */
{
    const t = new Int32Array(4);
    for (const k of canonical)
        assert(Object.getOwnPropertyDescriptor(t, k), undefined, `gopd ${k}`);
    assert(Object.getOwnPropertyNames(t).indexOf("NaN"), -1);
}

/* [[DefineOwnProperty]] fails */
{
    const t = new Int32Array(4);
    for (const k of canonical) {
        assert(Reflect.defineProperty(t, k, { value: 1 }), false,
               `defineProperty ${k}`);
        let threw = false;
        try {
            Object.defineProperty(t, k, { value: 1 });
        } catch (e) {
            threw = e instanceof TypeError;
        }
        assert(threw, true, `Object.defineProperty ${k}`);
    }
    /* the non canonical ones define normally */
    for (const k of ordinary)
        assert(Reflect.defineProperty(t, k, { value: 1 }), true,
               `defineProperty ${k}`);
}

/* [[Delete]] on an absent numeric index succeeds */
{
    const t = new Int32Array(4);
    for (const k of canonical)
        assert(delete t[k], true, `delete ${k}`);
    assert(delete t[NaN], true);
}

/* every typed array flavour shares the integer-indexed behaviour, including
   an out of bounds view, a zero length one and a bigint one */
{
    const ctors = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array,
                   Uint16Array, Int32Array, Uint32Array, Float16Array,
                   Float32Array, Float64Array, BigInt64Array, BigUint64Array];
    for (const Ctor of ctors) {
        const t = new Ctor(0);
        assert(t.NaN, undefined, Ctor.name);
        assert("NaN" in t, false, Ctor.name);
        assert(Reflect.defineProperty(t, "NaN", { value: 1 }), false, Ctor.name);
    }
}

/* only integer-indexed exotic objects are affected: ordinary objects, plain
   arrays and strings keep "NaN" as an ordinary property */
{
    const o = { NaN: 1 };
    assert(o.NaN, 1);
    assert("NaN" in o, true);
    assert(Reflect.defineProperty(o, "NaN", { value: 2 }), true);
    assert(o.NaN, 2);

    const a = [];
    a.NaN = 3;
    assert(a.NaN, 3);
    assert("NaN" in a, true);
    assert(a.length, 0);

    assert(Object.getOwnPropertyDescriptor("abc", "NaN"), undefined);
    assert("NaN" in Object("abc"), false);
    const so = Object("abc");
    so.NaN = 4;
    assert(so.NaN, 4);

    /* an ArrayBuffer is not integer-indexed either */
    const b = new ArrayBuffer(8);
    b.NaN = 5;
    assert(b.NaN, 5);
}

/* A canonical numeric index is not a property, so a store that requires the
   property to already exist has nothing to write to. That is what a `with`
   binding deleted between the reference being taken and the store looks
   like: SetMutableBinding throws a ReferenceError rather than letting the
   integer-indexed path swallow the write. `with` needs sloppy mode, which a
   module is not, so the scenario is built through an indirect eval. */
{
    function vanishing(name) {
        const key = JSON.stringify(name);
        return (0, eval)(`(function() {
            var env = Object.create(new Int32Array(10));
            Object.defineProperty(env, ${key}, { configurable: true, value: 100 });
            var caught = null;
            with (env) {
                try {
                    (function() {
                        "use strict";
                        ${name} = (delete env[${key}], 0);
                    })();
                } catch (e) { caught = e; }
            }
            return [caught, Object.getOwnPropertyDescriptor(env, ${key})];
        })()`);
    }

    for (const name of ["NaN", "Infinity"]) {
        const [caught, desc] = vanishing(name);
        assert(caught instanceof ReferenceError, true, name);
        assert(desc, undefined, name);
    }

    /* an ordinary property name reaches the same ReferenceError by the
       ordinary route, so the two agree */
    const [caught, desc] = vanishing("ordinary");
    assert(caught instanceof ReferenceError, true, "ordinary");
    assert(desc, undefined, "ordinary");
}
