// `set Iterator.prototype.constructor` must implement
// SetterThatIgnoresPrototypeProperties(%Iterator.prototype%, "constructor", v):
// it operates on `this` (the receiver), not on the value being assigned.
// https://tc39.es/ecma262/#sec-set-iterator.prototype.constructor
// https://tc39.es/ecma262/#sec-SetterThatIgnoresPrototypeProperties
import { assert, assertThrows } from "./assert.js";

const desc = Object.getOwnPropertyDescriptor(Iterator.prototype, "constructor");
const get = desc.get;
const set = desc.set;

assert(typeof get, "function");
assert(typeof set, "function");
assert(get !== set, true);
assert(get.length, 0);
assert(set.length, 1);

assert(get.call({}), Iterator);
assert(get.call(undefined), Iterator);
assert(Iterator.prototype.constructor, Iterator);
{
    const o = {};
    assert(get.call(o, 123), Iterator);
    assert(Object.prototype.hasOwnProperty.call(o, "constructor"), false);
}

{
    const o = {};
    assert(set.call(o), undefined);
    const d = Object.getOwnPropertyDescriptor(o, "constructor");
    assert(d.value, undefined);
    assert(d.writable, true);
    assert(d.enumerable, true);
    assert(d.configurable, true);
}

{
    const o = {};
    assert(set.call(o, 0), undefined);
    // The value `v` need not be an object; a fresh data property is created.
    const d = Object.getOwnPropertyDescriptor(o, "constructor");
    assert(d.value, 0);
    assert(d.writable, true);
    assert(d.enumerable, true);
    assert(d.configurable, true);
}

assertThrows(TypeError, () => set.call(undefined, 0));
assertThrows(TypeError, () => set.call(null, 0));
assertThrows(TypeError, () => set.call(1, 0));
assertThrows(TypeError, () => set.call("s", 0));

assertThrows(TypeError, () => set.call(Iterator.prototype, 0));
assert(Iterator.prototype.constructor, Iterator);

{
    const o = {};
    Object.defineProperty(o, "constructor", {
        value: 1, writable: false, enumerable: true, configurable: true,
    });
    assertThrows(TypeError, () => set.call(o, 2));
    assert(o.constructor, 1); // unchanged
}

{
    const o = { constructor: 1 };
    assert(set.call(o, 2), undefined);
    assert(o.constructor, 2);
}

{
    const o = Object.preventExtensions({});
    assertThrows(TypeError, () => set.call(o, 0));
    assert(Object.prototype.hasOwnProperty.call(o, "constructor"), false);
}

{
    const it = Object.create(Iterator.prototype);
    it.constructor = 42;
    const d = Object.getOwnPropertyDescriptor(it, "constructor");
    assert(d.value, 42);
    assert(d.writable, true);
    assert(d.enumerable, true);
    assert(d.configurable, true);
    assert(Iterator.prototype.constructor, Iterator);
}

