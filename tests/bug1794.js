import { assert, assertThrows } from "./assert.js";

// Each key is checked for enumerability and its descriptor read before
// the next key is looked at.
let log = [];
const props = new Proxy({ a: { value: 1 }, b: { value: 2 } }, {
    ownKeys(t) { log.push("ownKeys"); return Reflect.ownKeys(t); },
    getOwnPropertyDescriptor(t, k) { log.push("gopd " + k); return Reflect.getOwnPropertyDescriptor(t, k); },
    get(t, k) { log.push("get " + k); return Reflect.get(t, k); },
});
let o = Object.defineProperties({}, props);
assert(log.join(), "ownKeys,gopd a,get a,gopd b,get b");
assert(o.a, 1);
assert(o.b, 2);

log = [];
o = Object.create(null, props);
assert(log.join(), "ownKeys,gopd a,get a,gopd b,get b");
assert(o.a, 1);
assert(o.b, 2);

// All descriptors are validated before any property is defined.
o = {};
assertThrows(TypeError, () => Object.defineProperties(o, { a: { value: 1 }, b: { get: 1 } }));
assert("a" in o, false);
assertThrows(TypeError, () => Object.defineProperties(o, { a: { value: 1 }, b: 2 }));
assert("a" in o, false);

// A getter that makes a later key non-enumerable or deletes it is seen.
const src = {
    get a() {
        Object.defineProperty(src, "b", { enumerable: false });
        delete src.c;
        return { value: 1 };
    },
    b: { value: 2 },
    c: { value: 3 },
    d: { value: 4 },
};
o = Object.defineProperties({}, src);
assert(Object.keys(o).length, 0);
assert(o.a, 1);
assert("b" in o, false);
assert("c" in o, false);
assert(o.d, 4);

// Non-enumerable and symbol keys.
const sym = Symbol("s");
const src2 = { [sym]: { value: "s" } };
Object.defineProperty(src2, "hidden", { value: { value: "h" }, enumerable: false });
o = Object.defineProperties({}, src2);
assert(o[sym], "s");
assert("hidden" in o, false);
