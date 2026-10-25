import { assert, assertThrows } from "./assert.js";

// %TypedArray%.prototype.{slice,filter,map,subarray} with species constructor
// converting between BigInt and Number content types must throw TypeError.

class BigIntSpeciesNum extends BigInt64Array {
    static [Symbol.species] = Uint8Array;
}

class BigUintSpeciesNum extends BigUint64Array {
    static [Symbol.species] = Float64Array;
}

class NumSpeciesBigInt extends Uint8Array {
    static [Symbol.species] = BigInt64Array;
}

class NumSpeciesBigUint extends Float64Array {
    static [Symbol.species] = BigUint64Array;
}

class BigIntSpeciesBigInt extends BigInt64Array {
    static [Symbol.species] = BigUint64Array;
}

class BigUintSpeciesBigInt extends BigUint64Array {
    static [Symbol.species] = BigInt64Array;
}

class NumSpeciesNum extends Int8Array {
    static [Symbol.species] = Uint8Array;
}

// 1. BigInt -> Number (empty and non-empty)
for (const cls of [BigIntSpeciesNum, BigUintSpeciesNum]) {
    for (const a of [new cls(), new cls(2)]) {
        assertThrows(TypeError, () => a.slice());
        assertThrows(TypeError, () => a.filter(() => true));
        assertThrows(TypeError, () => a.map(x => x));
        assertThrows(TypeError, () => a.subarray());
    }
}

// 2. Number -> BigInt (empty and non-empty)
for (const b of [new NumSpeciesBigInt(), new NumSpeciesBigInt(2)]) {
    assertThrows(TypeError, () => b.slice());
    assertThrows(TypeError, () => b.filter(() => true));
    assertThrows(TypeError, () => b.map(x => x));
}
// For Uint8Array -> BigInt64Array subarray, the buffer must be large enough
// to construct the target BigInt64Array view, otherwise a RangeError is thrown
// before TypedArraySpeciesCreate can check the ContentType.
assertThrows(TypeError, () => new NumSpeciesBigInt().subarray());
assertThrows(TypeError, () => new NumSpeciesBigInt(16).subarray(0, 2));

for (const b of [new NumSpeciesBigUint(), new NumSpeciesBigUint(2)]) {
    assertThrows(TypeError, () => b.slice());
    assertThrows(TypeError, () => b.filter(() => true));
    assertThrows(TypeError, () => b.map(x => x));
    assertThrows(TypeError, () => b.subarray());
}

// 3. Dynamic species constructor via property override
{
    const a = new BigInt64Array(2);
    a.constructor = { [Symbol.species]: Uint8Array };
    assertThrows(TypeError, () => a.slice());
    assertThrows(TypeError, () => a.filter(() => true));
    assertThrows(TypeError, () => a.map(x => x));
    assertThrows(TypeError, () => a.subarray());
}

{
    const a = new Uint8Array(2);
    a.constructor = { [Symbol.species]: BigInt64Array };
    assertThrows(TypeError, () => a.slice());
    assertThrows(TypeError, () => a.filter(() => true));
    assertThrows(TypeError, () => a.map(x => x));
}
{
    const a = new Uint8Array(16);
    a.constructor = { [Symbol.species]: BigInt64Array };
    assertThrows(TypeError, () => a.subarray(0, 2));
}

{
    const a = new BigUint64Array(2);
    a.constructor = { [Symbol.species]: function() { return new Float64Array(2); } };
    assertThrows(TypeError, () => a.slice());
    assertThrows(TypeError, () => a.filter(() => true));
    assertThrows(TypeError, () => a.map(x => x));
    assertThrows(TypeError, () => a.subarray());
}

{
    const a = new Float64Array(2);
    a.constructor = { [Symbol.species]: function() { return new BigUint64Array(2); } };
    assertThrows(TypeError, () => a.slice());
    assertThrows(TypeError, () => a.filter(() => true));
    assertThrows(TypeError, () => a.map(x => x));
    assertThrows(TypeError, () => a.subarray());
}

// 4. Compatible species: BigInt -> BigInt
for (const cls of [BigIntSpeciesBigInt, BigUintSpeciesBigInt]) {
    for (const c of [new cls(), new cls(2)]) {
        assert(c.slice() instanceof (cls === BigIntSpeciesBigInt ? BigUint64Array : BigInt64Array));
        assert(c.filter(() => true) instanceof (cls === BigIntSpeciesBigInt ? BigUint64Array : BigInt64Array));
        assert(c.map(x => x) instanceof (cls === BigIntSpeciesBigInt ? BigUint64Array : BigInt64Array));
        assert(c.subarray() instanceof (cls === BigIntSpeciesBigInt ? BigUint64Array : BigInt64Array));
    }
}

// 5. Compatible species: Number -> Number
for (const d of [new NumSpeciesNum(), new NumSpeciesNum(2)]) {
    assert(d.slice() instanceof Uint8Array);
    assert(d.filter(() => true) instanceof Uint8Array);
    assert(d.map(x => x) instanceof Uint8Array);
    assert(d.subarray() instanceof Uint8Array);
}
