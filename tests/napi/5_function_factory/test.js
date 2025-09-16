'use strict';
const addon = napi(`./binding.${ext}`);

const fn = addon();
assert.strictEqual(fn(), 'hello world'); // 'hello world'
