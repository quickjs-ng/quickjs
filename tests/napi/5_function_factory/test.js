'use strict';
const addon = await import(`binding.${ext}`);

const fn = addon();
assert.strictEqual(fn(), 'hello world'); // 'hello world'
