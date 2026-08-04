import { assert, assertArrayEquals, assertThrows } from "./assert.js";

/* The post-trap invariant checks of the proxy [[DefineOwnProperty]],
   [[GetOwnProperty]] and [[HasProperty]] internal methods must obtain the
   target's extensibility through IsExtensible(). When the target is itself a
   proxy that is observable: its isExtensible trap runs, and it can throw. */

function innerProxy(target, isExtensible) {
    return new Proxy(target, { isExtensible });
}

/* A non-callable isExtensible trap on the target proxy must surface as a
   TypeError instead of being silently ignored. */
assertThrows(TypeError, () => Reflect.defineProperty(
    new Proxy(innerProxy({}, 0), { defineProperty: () => true }), "x", {}));

/* IsExtensible(target) is performed whether or not the target already has
   the property. */
assertThrows(TypeError, () => Reflect.defineProperty(
    new Proxy(innerProxy({ x: 1 }, 0), { defineProperty: () => true }), "x", {}));

assertThrows(TypeError, () => Reflect.getOwnPropertyDescriptor(
    new Proxy(innerProxy({ x: 1 }, 0), { getOwnPropertyDescriptor: () => undefined }), "x"));

assertThrows(TypeError, () => Reflect.has(
    new Proxy(innerProxy({ x: 1 }, 0), { has: () => false }), "x"));

/* An isExtensible trap that lies about an extensible target is rejected. */
assertThrows(TypeError, () => Reflect.defineProperty(
    new Proxy(innerProxy({}, () => false), { defineProperty: () => true }), "x", {}));

/* An exception thrown by the trap propagates unchanged. */
for (const op of [
    (p) => Reflect.defineProperty(new Proxy(p, { defineProperty: () => true }), "x", {}),
    (p) => Reflect.getOwnPropertyDescriptor(new Proxy(p, { getOwnPropertyDescriptor: () => undefined }), "x"),
    (p) => Reflect.has(new Proxy(p, { has: () => false }), "x"),
]) {
    assertThrows(RangeError, () => op(innerProxy({ x: 1 }, () => { throw new RangeError(); })));
}

/* A well-behaved target proxy still allows the operations to complete, and the
   traps are called in the order the spec prescribes: [[GetOwnProperty]] on the
   target first, then IsExtensible(target). */
function trapLog(target, op, expected) {
    const log = [];
    const p = new Proxy(target, {
        getOwnPropertyDescriptor(t, k) { log.push("gOPD"); return Reflect.getOwnPropertyDescriptor(t, k); },
        isExtensible(t) { log.push("isExtensible"); return Reflect.isExtensible(t); },
    });
    assert(op(p), expected);
    return log;
}

assertArrayEquals(trapLog({ x: 1 }, (p) => Reflect.defineProperty(
    new Proxy(p, { defineProperty: () => true }), "x", { value: 2 }), true), ["gOPD", "isExtensible"]);

assertArrayEquals(trapLog({ x: 1 }, (p) => Reflect.getOwnPropertyDescriptor(
    new Proxy(p, { getOwnPropertyDescriptor: () => undefined }), "x"), undefined), ["gOPD", "isExtensible"]);

assertArrayEquals(trapLog({ x: 1 }, (p) => Reflect.has(
    new Proxy(p, { has: () => false }), "x"), false), ["gOPD", "isExtensible"]);

/* When the property is absent from the target, the configurability check
   cannot fail, so [[HasProperty]] and [[GetOwnProperty]] stop before
   IsExtensible(). [[DefineOwnProperty]] always performs it. */
assertArrayEquals(trapLog({}, (p) => Reflect.has(
    new Proxy(p, { has: () => false }), "x"), false), ["gOPD"]);

assertArrayEquals(trapLog({}, (p) => Reflect.getOwnPropertyDescriptor(
    new Proxy(p, { getOwnPropertyDescriptor: () => undefined }), "x"), undefined), ["gOPD"]);

assertArrayEquals(trapLog({}, (p) => Reflect.defineProperty(
    new Proxy(p, { defineProperty: () => true }), "x", { value: 1 }), true), ["gOPD", "isExtensible"]);

