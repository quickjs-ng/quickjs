'use strict';
const addon = napi(`./binding.${ext}`);

assert.strictEqual(addon.add(3, 5), 8);
