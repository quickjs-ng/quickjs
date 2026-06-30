// `set Iterator.prototype[Symbol.toStringTag]` implements
// SetterThatIgnoresPrototypeProperties(%Iterator.prototype%, @@toStringTag, v).
// In particular the CreateDataPropertyOrThrow step must throw on a
// non-extensible receiver rather than silently doing nothing.
// https://tc39.es/ecma262/#sec-set-iterator.prototype-%symbol.tostringtag%
// https://tc39.es/ecma262/#sec-SetterThatIgnoresPrototypeProperties
import { assert, assertThrows } from "./assert.js";

const desc = Object.getOwnPropertyDescriptor(Iterator.prototype, Symbol.toStringTag);
const set = desc.set;

assert(Iterator.prototype[Symbol.toStringTag], "Iterator");

assertThrows(TypeError, () => set.call(undefined, "x"));
assertThrows(TypeError, () => set.call(null, "x"));
assertThrows(TypeError, () => set.call(1, "x"));

assertThrows(TypeError, () => set.call(Iterator.prototype, "x"));
assert(Iterator.prototype[Symbol.toStringTag], "Iterator");

{
    const o = {};
    assert(set.call(o, "x"), undefined);
    const d = Object.getOwnPropertyDescriptor(o, Symbol.toStringTag);
    assert(d.value, "x");
    assert(d.writable, true);
    assert(d.enumerable, true);
    assert(d.configurable, true);
}

{
    const o = {};
    Object.defineProperty(o, Symbol.toStringTag, {
        value: "a", writable: false, enumerable: true, configurable: true,
    });
    assertThrows(TypeError, () => set.call(o, "b"));
    assert(o[Symbol.toStringTag], "a");
}

{
    const o = Object.preventExtensions({});
    assertThrows(TypeError, () => set.call(o, "x"));
    assert(Object.prototype.hasOwnProperty.call(o, Symbol.toStringTag), false);
}
