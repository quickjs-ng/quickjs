'use strict';
const addon = napi(`./binding.${ext}`);

const obj1 = addon.createObject(10);
const obj2 = addon.createObject(20);
const result = addon.add(obj1, obj2);
assert.strictEqual(result, 30);
