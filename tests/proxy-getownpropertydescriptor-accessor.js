// Proxy [[GetOwnProperty]] must return CompletePropertyDescriptor(resultDesc):
// an accessor descriptor produced by a getOwnPropertyDescriptor trap must
// surface as an accessor (get/set), not degrade to
// { value: undefined, writable: false }.
// https://tc39.es/ecma262/#sec-proxy-object-internal-methods-and-internal-slots-getownproperty-p
import { assert } from "./assert.js";

const getX = function() { return 42; };
const target = {};
Object.defineProperty(target, "x", {
    configurable: true,
    enumerable: true,
    get: getX,
});

// accessor descriptor through the trap
{
    const p = new Proxy(target, {
        getOwnPropertyDescriptor: Reflect.getOwnPropertyDescriptor,
    });
    const d = Object.getOwnPropertyDescriptor(p, "x");
    assert(d.get, getX);
    assert(d.set, undefined);
    assert("value" in d, false);
    assert("writable" in d, false);
    assert(d.enumerable, true);
    assert(d.configurable, true);
    assert(p.x, 42);
}

// incomplete accessor descriptor from the trap: absent fields default
// per CompletePropertyDescriptor
{
    const p = new Proxy(target, {
        getOwnPropertyDescriptor() {
            return { configurable: true, get: getX };
        },
    });
    const d = Object.getOwnPropertyDescriptor(p, "x");
    assert(d.get, getX);
    assert(d.set, undefined);
    assert(d.enumerable, false);
    assert(d.configurable, true);
}

// data descriptor through the trap is unaffected
{
    const t = { y: 7 };
    const p = new Proxy(t, {
        getOwnPropertyDescriptor: Reflect.getOwnPropertyDescriptor,
    });
    const d = Object.getOwnPropertyDescriptor(p, "y");
    assert(d.value, 7);
    assert(d.writable, true);
    assert(d.enumerable, true);
    assert(d.configurable, true);
    assert("get" in d, false);
    assert("set" in d, false);
}
