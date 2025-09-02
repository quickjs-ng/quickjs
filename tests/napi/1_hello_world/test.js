'use strict';
const addon = napi(`./binding.${ext}`);

assert.strictEqual(addon.hello(), 'world');
