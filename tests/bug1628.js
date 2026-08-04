import { assert, assertThrows } from "./assert.js";

/* The ownKeys trap result is fed to CreateListFromArrayLike(), which throws
   a TypeError when the result is not an object. */

const primitives = [0, 1, -1, NaN, "", "ab", true, false, undefined, null, 1n,
                    Symbol("s")];

for (const v of primitives) {
    const p = new Proxy({}, { ownKeys() { return v; } });
    assertThrows(TypeError, function() { Reflect.ownKeys(p); });
    assertThrows(TypeError, function() { Object.keys(p); });
    assertThrows(TypeError, function() { Object.getOwnPropertyNames(p); });
    assertThrows(TypeError, function() { Object.getOwnPropertySymbols(p); });
    assertThrows(TypeError, function() { Object.assign({}, p); });
    assertThrows(TypeError, function() { ({...p}); });
    assertThrows(TypeError, function() { JSON.stringify(p); });
    assertThrows(TypeError, function() { for (const k in p) ; });
}

{
    const p = new Proxy({}, { ownKeys() { return "ab"; } });
    assertThrows(TypeError, function() { Reflect.ownKeys(p); });
}

{
    assert(Reflect.ownKeys(new Proxy({}, { ownKeys: () => [] })).length, 0);

    const a = Reflect.ownKeys(new Proxy({}, { ownKeys: () => ["a", "b"] }));
    assert(a.length, 2);
    assert(a[0], "a");
    assert(a[1], "b");

    const b = Reflect.ownKeys(new Proxy({}, {
        ownKeys: () => ({length: 2, 0: "x", 1: "y"}),
    }));
    assert(b.length, 2);
    assert(b[0], "x");
    assert(b[1], "y");

    /* a function has no 'length' index properties, so an empty list */
    assert(Reflect.ownKeys(new Proxy({}, { ownKeys: () => function(){} })).length, 0);
}

{
    let called = 0;
    const p = new Proxy({}, { ownKeys() { called++; return 0; } });
    assertThrows(TypeError, function() { Reflect.ownKeys(p); });
    assert(called, 1);

    const q = new Proxy({}, { ownKeys() { throw new RangeError("boom"); } });
    assertThrows(RangeError, function() { Reflect.ownKeys(q); });
}
