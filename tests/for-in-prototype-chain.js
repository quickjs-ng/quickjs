import { assert } from "./assert.js";

/* for-in walks the prototype chain. The fast path for building the iterator
   only looks at an object's shape, which holds every enumerable string keyed
   own property *unless* the object is exotic: arrays, arguments objects,
   typed arrays, string objects, proxies and module namespaces keep some or
   all of their properties elsewhere and have to be enumerated the slow way. */

function keys(o) {
    const r = [];
    for (const k in o)
        r.push(k);
    return r.join(",");
}

/* nothing enumerable above: only the own keys show up */
{
    const o = { a: 1, b: 2 };
    assert(keys(o), "a,b");
    assert(keys(Object.create(null)), "");

    const bare = Object.create(null);
    bare.a = 1;
    bare.b = 2;
    assert(keys(bare), "a,b");
}

/* a plain prototype contributes its enumerable string keys, after the own
   ones, and only once when shadowed */
{
    const proto = { pa: 1, pb: 2 };
    const o = Object.create(proto);
    o.a = 1;
    assert(keys(o), "a,pa,pb");

    const shadow = Object.create({ a: 1, b: 2 });
    shadow.a = 9;
    assert(keys(shadow), "a,b");

    /* a non-enumerable own property hides the inherited one entirely */
    const hidden = Object.create({ a: 1, b: 2 });
    Object.defineProperty(hidden, "a", { value: 9, enumerable: false });
    assert(keys(hidden), "b");
}

/* what the prototype scan must treat as "nothing to see here" */
{
    /* non-enumerable only */
    const proto = {};
    Object.defineProperty(proto, "hidden", { value: 1, enumerable: false });
    const o = Object.create(proto);
    o.a = 1;
    assert(keys(o), "a");

    /* symbol keyed only: for-in never yields symbols */
    const symProto = {};
    symProto[Symbol("s")] = 1;
    Object.defineProperty(symProto, Symbol("t"), { value: 1, enumerable: true });
    const so = Object.create(symProto);
    so.a = 1;
    assert(keys(so), "a");

    /* deleted properties leave holes in the shape that must be skipped */
    const gappy = { x: 1, y: 2, z: 3 };
    delete gappy.x;
    delete gappy.y;
    delete gappy.z;
    const go = Object.create(gappy);
    go.a = 1;
    assert(keys(go), "a");

    /* ... and a hole next to a live enumerable property is still found */
    const partly = { x: 1, keep: 2 };
    delete partly.x;
    const po = Object.create(partly);
    po.a = 1;
    assert(keys(po), "a,keep");

    /* an accessor is enumerable like any other property */
    const accProto = { get g() { return 1; } };
    assert(keys(Object.create(accProto)), "g");
}

/* the exotic prototypes: their properties live outside the shape */
{
    /* array elements */
    assert(keys(Object.create([10, 20, 30])), "0,1,2");
    assert(keys(Object.create([])), "");
    const sparse = Object.create([1, , 3]);
    assert(keys(sparse), "0,2");

    /* an own key in front of an array prototype */
    const oa = Object.create([10, 20]);
    oa.a = 1;
    assert(keys(oa), "a,0,1");

    /* string object index properties */
    assert(keys(Object.create(new String("ab"))), "0,1");
    assert(keys(new String("ab")), "0,1");

    /* typed array indices */
    assert(keys(Object.create(new Int8Array(3))), "0,1,2");
    assert(keys(Object.create(new Float64Array(0))), "");

    /* arguments objects, mapped and unmapped */
    assert(keys(Object.create((function() { return arguments; })(1, 2))), "0,1");
    assert(keys(Object.create((function() {
        "use strict";
        return arguments;
    })(1, 2, 3))), "0,1,2");
}

/* a proxy prototype must go through its traps, and only the enumerable keys
   they report come out */
{
    const seen = [];
    const target = { a: 1, b: 2 };
    Object.defineProperty(target, "hidden", { value: 3, enumerable: false });
    const proto = new Proxy(target, {
        ownKeys(t) { seen.push("ownKeys"); return Reflect.ownKeys(t); },
        getOwnPropertyDescriptor(t, k) {
            seen.push("gopd:" + k);
            return Reflect.getOwnPropertyDescriptor(t, k);
        },
    });
    const o = Object.create(proto);
    o.own = 1;
    assert(keys(o), "own,a,b");
    assert(seen.indexOf("ownKeys") >= 0, true);
    assert(seen.indexOf("gopd:hidden") >= 0, true);
}

/* a module namespace prototype: its exports are enumerable */
{
    const ns = await import("./fixture_string_exports.js");
    const o = Object.create(ns);
    o.own = 1;
    const got = keys(o).split(",");
    assert(got[0], "own");
    assert(got.indexOf("regularExport") > 0, true);
    assert(got.indexOf("string-export-1") > 0, true);
    assert(got.indexOf("normalName") > 0, true);
}

/* built-in prototypes carry nothing enumerable */
{
    assert(keys(Object.create(new Error("m"))), "");
    assert(keys(Object.create(new Date())), "");
    assert(keys(Object.create(new Map([[1, 2]]))), "");
    assert(keys(Object.create(/x/g)), "");
    assert(keys(Object.create(new Number(1))), "");
    assert(keys(Object.create(function f() {})), "");

    /* ... unless something is put there */
    function f() {}
    f.pub = 1;
    assert(keys(Object.create(f)), "pub");

    const e = new Error("m");
    e.detail = 1;
    assert(keys(Object.create(e)), "detail");
}

/* a long chain, and a chain that mixes exotic and plain links */
{
    let p = Object.create(null);
    p.deep = 1;
    for (let i = 0; i < 8; i++)
        p = Object.create(p);
    const o = Object.create(p);
    o.own = 1;
    assert(keys(o), "own,deep");

    const mixed = Object.create(Object.create([1, 2]));
    mixed.own = 1;
    assert(keys(mixed), "own,0,1");
}

/* extending Object.prototype is visible from every for-in */
{
    Object.prototype.__forInExt = 1;
    try {
        assert(keys({ a: 1 }), "a,__forInExt");
        assert(keys(Object.create(null)), "");
    } finally {
        delete Object.prototype.__forInExt;
    }
    assert(keys({ a: 1 }), "a");
}

/* primitives and nullish values */
{
    assert(keys(null), "");
    assert(keys(undefined), "");
    assert(keys("ab"), "0,1");
    assert(keys(1), "");
    assert(keys(true), "");
    assert(keys(Symbol("s")), "");
}

/* the iterator is built once, up front: adding to the prototype during the
   loop does not extend it, and deleting an own key still skips it */
{
    const proto = { p1: 1 };
    const o = Object.create(proto);
    o.a = 1;
    const got = [];
    for (const k in o) {
        got.push(k);
        proto.p2 = 2;
    }
    delete proto.p2;
    assert(got.join(","), "a,p1");

    const o2 = { a: 1, b: 2, c: 3 };
    const got2 = [];
    for (const k in o2) {
        got2.push(k);
        delete o2.b;
    }
    assert(got2.join(","), "a,c");
}
