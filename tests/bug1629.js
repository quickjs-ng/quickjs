import { assert, assertThrows } from "./assert.js";

// The descriptor returned by the "getOwnPropertyDescriptor" trap is
// completed and then checked with IsCompatiblePropertyDescriptor()
// against the target's own property.

function proxy(target, desc) {
    return new Proxy(target, { getOwnPropertyDescriptor() { return desc; } });
}

function gopd(target, desc) {
    return Object.getOwnPropertyDescriptor(proxy(target, desc), "x");
}

const g = function() {};
const g2 = function() {};
const s = function() {};

// Non-configurable, non-writable data property: the value cannot differ.
{
    const t = Object.defineProperty({}, "x", { value: 1 });
    assertThrows(TypeError, () => gopd(t, { value: 2 }));
    // A completed descriptor always has a value, so an empty one reports
    // "undefined" and is incompatible too.
    assertThrows(TypeError, () => gopd(t, {}));
    assertThrows(TypeError, () => gopd(t, { value: 1, enumerable: true }));
    // Reporting the same value is fine.
    const d = gopd(t, { value: 1 });
    assert(d.value, 1);
    assert(d.writable, false);
    assert(d.enumerable, false);
    assert(d.configurable, false);
}

// Non-configurable but writable: the value may differ, but the property
// cannot be reported as non-writable.
{
    const t = Object.defineProperty({}, "x", { value: 1, writable: true });
    assert(gopd(t, { value: 2, writable: true }).value, 2);
    assertThrows(TypeError, () => gopd(t, { value: 1 }));
}

// A non-configurable accessor cannot be reported with other accessors.
{
    const t = Object.defineProperty({}, "x", { get: g, set: s });
    assertThrows(TypeError, () => gopd(t, { get: g2, set: s }));
    assertThrows(TypeError, () => gopd(t, { get: g, set: g2 }));
    assertThrows(TypeError, () => gopd(t, { get: g }));
    assertThrows(TypeError, () => gopd(t, { value: 1 }));
    const d = gopd(t, { get: g, set: s });
    assert(d.get, g);
    assert(d.set, s);
}

// A non-configurable data property cannot be reported as an accessor.
{
    const t = Object.defineProperty({}, "x", { value: 1 });
    assertThrows(TypeError, () => gopd(t, { get: g }));
}

// A configurable property of the target puts no constraint on the value,
// but the trap still cannot report it as non-configurable.
{
    const t = Object.defineProperty({}, "x", { value: 1, configurable: true });
    assert(gopd(t, { value: 2, configurable: true }).value, 2);
    assertThrows(TypeError, () => gopd(t, { value: 2 }));
}

// An accessor descriptor accepted by the checks is reported as such and
// not silently turned into a data descriptor.
{
    const d = gopd({}, { get: g, set: s, configurable: true });
    assert(d.get, g);
    assert(d.set, s);
    assert("value" in d, false);
    assert("writable" in d, false);

    const p = proxy({}, { get: g, set: s, configurable: true });
    assert(p.__lookupGetter__("x"), g);
    assert(p.__lookupSetter__("x"), s);
}
