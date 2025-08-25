'use strict';
const addon = await import(`binding.${ext}`);

assert.strictEqual(addon.hello(), 'world');
