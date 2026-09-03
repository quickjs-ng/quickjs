import { assert } from "./assert.js";

/* ToPropertyKey on a computed key can run arbitrary user code, so where it
   happens is observable. A property *reference* keeps the key unconverted:
   the conversion is part of GetValue/PutValue, i.e. it happens after the
   assigned value has been evaluated. A computed key in an object *literal*
   is the other way around: it is converted before its value is evaluated. */

let log = [];
const note = (n, v) => { log.push(n); return v; };
const key = (n, v = n) => ({ toString() { log.push(n + "-tostring"); return v; } });

function target(name) {
    const o = {};
    Object.defineProperty(o, name, {
        set(v) { log.push("set"); },
        get() { log.push("get-target"); return 0; },
        configurable: true,
    });
    return o;
}

function order(fn) {
    log = [];
    try {
        fn();
    } catch (e) {
        log.push("throw:" + e.constructor.name);
    }
    return log.join(",");
}

/* obj[key] = value: the value comes first, the key conversion last */
{
    const o = {};
    assert(order(() => { o[note("key", key("k"))] = note("value", 1); }),
           "key,value,k-tostring");
    assert(o.k, 1);
}

/* super[key] = value takes the same path */
{
    const proto = target("sk");
    const o = {
        m() { super[note("key", key("sk"))] = note("value", 1); },
    };
    Object.setPrototypeOf(o, proto);
    assert(order(() => o.m()), "key,value,sk-tostring,set");
}

/* a value that throws means the key is never converted at all */
{
    const o = {};
    assert(order(() => {
        o[key("k")] = (() => { throw new RangeError("boom"); })();
    }), "throw:RangeError");
}

/* and a key conversion that throws still happens after the value ran */
{
    const o = {};
    const bad = {
        toString() { log.push("key-tostring"); throw new TypeError("bad key"); },
    };
    assert(order(() => { o[bad] = note("value", 1); }),
           "value,key-tostring,throw:TypeError");
}

/* an object literal converts the key before evaluating the value */
{
    let o;
    assert(order(() => {
        o = { [note("key", key("k"))]: note("value", 1) };
    }), "key,k-tostring,value");
    assert(o.k, 1);
}

/* a computed key is converted exactly once, even where the reference is both
   read and written */
{
    let calls = 0;
    const k = { toString() { calls++; return "k"; } };
    const o = { k: 1 };
    o[k] += 1;
    assert(calls, 1);
    assert(o.k, 2);

    calls = 0;
    o[k]++;
    assert(calls, 1);
    assert(o.k, 3);

    calls = 0;
    o[k] ||= 9;
    assert(calls, 1);
    assert(o.k, 3);

    calls = 0;
    o[k] = 5;
    assert(calls, 1);
    assert(o.k, 5);
}

/* Symbol.toPrimitive wins over toString, and is likewise called once */
{
    let calls = 0;
    const k = { [Symbol.toPrimitive]() { calls++; return "sk"; } };
    const o = {};
    o[k] = 1;
    assert(calls, 1);
    assert(o.sk, 1);
}

/* a symbol key needs no conversion at all */
{
    const s = Symbol("s");
    const o = {};
    assert(order(() => { o[note("key", s)] = note("value", 1); }), "key,value");
    assert(o[s], 1);
}

/* the reads and the compound forms keep converting the key up front, since
   the reference is dereferenced before the right hand side is evaluated */
{
    const o = { k: 1 };
    assert(order(() => { o[note("key", key("k"))] += note("value", 1); }),
           "key,k-tostring,value");
    const falsy = { k: 0 };
    assert(order(() => { falsy[note("key", key("k"))] ||= note("value", 1); }),
           "key,k-tostring,value");
    assert(order(() => { o[note("key", key("k"))]; }), "key,k-tostring");
    assert(order(() => { o[note("key", key("k"))]++; }), "key,k-tostring");
    assert(order(() => { delete o[note("key", key("k"))]; }), "key,k-tostring");
}

/* a computed destructuring *target* is a reference too: the key is converted
   after the source property has been read */
{
    const t = target("tk");
    const src = { get p() { log.push("get-source"); return 1; } };
    assert(order(() => {
        ({ p: t[note("target-key", key("tk"))] } = note("source", src));
    }), "source,target-key,get-source,tk-tostring,set");
}

/* ... and after a default value has been evaluated */
{
    const t = target("tk");
    assert(order(() => {
        ({ p: t[note("target-key", key("tk"))] = note("default", 9) } =
            note("source", {}));
    }), "source,target-key,default,tk-tostring,set");
}

/* the array pattern converts the key after the iterator step */
{
    const t = target("tk");
    const src = {
        [Symbol.iterator]() {
            log.push("iterator");
            return { next() { log.push("next"); return { value: 1, done: false }; } };
        },
    };
    assert(order(() => {
        [t[note("target-key", key("tk"))]] = note("source", src);
    }), "source,iterator,target-key,next,tk-tostring,set");
}

/* rest targets convert once, before the store */
{
    const t = target("tk");
    assert(order(() => {
        ({ ...t[note("target-key", key("tk"))] } = note("source", { a: 1 }));
    }), "source,target-key,tk-tostring,set");

    const t2 = target("tk");
    assert(order(() => {
        [...t2[note("target-key", key("tk"))]] = note("source", [1]);
    }), "source,target-key,tk-tostring,set");
}

/* the values actually land where they should */
{
    const t = {};
    ({ p: t[key("a")] } = { p: 1 });
    assert(t.a, 1);

    [t[key("b")]] = [2];
    assert(t.b, 2);

    ({ ...t[key("c")] } = { z: 3 });
    assert(t.c.z, 3);

    [...t[key("d")]] = [4, 5];
    assert(t.d.join(","), "4,5");

    ({ p: t[key("e")] = 6 } = {});
    assert(t.e, 6);

    const nested = {};
    ({ p: { q: nested[key("f")] } } = { p: { q: 7 } });
    assert(nested.f, 7);
}

/* the whole right hand side is evaluated before the pattern runs, and
   then each computed target converts its own key as the pattern reaches
   it */
{
    const o = {};
    assert(order(() => {
        [o[key("k1")], o[key("k2")], o[key("k3")]] =
            [note("v1", 1), note("v2", 2), note("v3", 3)];
    }), "v1,v2,v3,k1-tostring,k2-tostring,k3-tostring");
    assert(o.k1 + "," + o.k2 + "," + o.k3, "1,2,3");

    const p = {};
    assert(order(() => {
        ({ a: p[key("ka")], b: p[key("kb")] } =
            { a: note("va", 1), b: note("vb", 2) });
    }), "va,vb,ka-tostring,kb-tostring");
    assert(p.ka + "," + p.kb, "1,2");
}

/* a nested pattern converts the outer target's key before it descends */
{
    const o = {};
    assert(order(() => {
        [[o[key("inner")]], o[key("outer")]] = [[note("vi", 1)], note("vo", 2)];
    }), "vi,vo,inner-tostring,outer-tostring");
    assert(o.inner + "," + o.outer, "1,2");
}

/* a computed target in a for-of head is re-evaluated and re-converted on
   every iteration */
{
    const o = {};
    let n = 0;
    assert(order(() => {
        for ([o[key("k" + n, "k")]] of [[1], [2]])
            n++;
    }), "k0-tostring,k1-tostring");
    assert(o.k, 2);

    const p = {};
    n = 0;
    assert(order(() => {
        for ({ v: p[key("j" + n, "j")] } of [{ v: 1 }, { v: 2 }])
            n++;
    }), "j0-tostring,j1-tostring");
    assert(p.j, 2);
}

/* the same key object used for both the source and the target is converted
   once for each of them */
{
    const src = { k: 7 };
    const dst = {};
    const k = key("k", "k");
    assert(order(() => {
        ({ [k]: dst[k] } = src);
    }), "k-tostring,k-tostring");
    assert(dst.k, 7);
}

/* a target whose setter runs user code sees the value only after the key
   has been converted */
{
    const o = target("k");
    assert(order(() => {
        o[key("k", "k")] = note("v", 1);
    }), "v,k-tostring,set");

    const o2 = target("k");
    assert(order(() => {
        [o2[key("k", "k")]] = [note("v", 1)];
    }), "v,k-tostring,set");

    const o3 = target("k");
    assert(order(() => {
        ({ p: o3[key("k", "k")] } = { p: note("v", 1) });
    }), "v,k-tostring,set");
}

/* a default value is evaluated before the key it will be stored under */
{
    const o = {};
    assert(order(() => {
        ({ p: o[key("k")] = note("dflt", 9) } = {});
    }), "dflt,k-tostring");
    assert(o.k, 9);

    const p = {};
    assert(order(() => {
        [p[key("k")] = note("dflt", 9)] = [];
    }), "dflt,k-tostring");
    assert(p.k, 9);
}
