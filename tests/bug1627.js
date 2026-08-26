import { assert, assertThrows } from "./assert.js";

// ToPropertyDescriptor must propagate an abrupt completion from Get(Obj, "get")
// or Get(Obj, "set") instead of reporting the callability check.

assertThrows(ReferenceError, () => {
    Object.create([], { x: { get get() { unresolvable; } } });
});

assertThrows(ReferenceError, () => {
    Object.create([], { x: { get set() { unresolvable; } } });
});

for (const key of ["get", "set"]) {
    const sentinel = new Error("thrown from the " + key + " accessor");
    const desc = { get [key]() { throw sentinel; } };
    let caught;
    try {
        Object.defineProperty({}, "p", desc);
    } catch (e) {
        caught = e;
    }
    assert(caught, sentinel, "abrupt completion of Get(Obj, \"" + key + "\")");
}

// A non-callable getter/setter still yields a TypeError.
assertThrows(TypeError, () => Object.defineProperty({}, "p", { get: 1 }));
assertThrows(TypeError, () => Object.defineProperty({}, "p", { set: 1 }));

// Accessors that do return a function keep working.
const o = {};
Object.defineProperty(o, "p", { get get() { return () => 42; } });
assert(o.p, 42);
