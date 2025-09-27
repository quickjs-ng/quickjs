'use strict';
const addon = napi(`./binding.${ext}`);

const obj1 = addon('hello');
const obj2 = addon('world');
assert.strictEqual(`${obj1.msg} ${obj2.msg}`, 'hello world');
