import { assert, assertThrows } from "./assert.js";

const target = {};
Object.defineProperty(target, "x", {
    value: 0,
    writable: true,
    configurable: false,
});
const proxy = new Proxy(target, {
    defineProperty: (object, key, descriptor) =>
        Reflect.defineProperty(object, key, descriptor),
});

assert(Reflect.defineProperty(proxy, "x", { value: 1 }));
assert(target.x, 1);

const array = new Proxy([0], {
    defineProperty: (object, key, descriptor) =>
        Reflect.defineProperty(object, key, descriptor),
});
assert(array.pop(), 0);
assert(array.length, 0);

const targetWithWritableProperty = {};
Object.defineProperty(targetWithWritableProperty, "x", {
    value: 0,
    writable: true,
    configurable: false,
});
const proxyWithFalseWritable = new Proxy(targetWithWritableProperty, {
    defineProperty: () => true,
});
assertThrows(TypeError, () =>
    Reflect.defineProperty(proxyWithFalseWritable, "x", { writable: false })
);
