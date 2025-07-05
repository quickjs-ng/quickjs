'use strict';
const addon = await import(`binding.${ext}`);

assert.strictEqual(addon.add(3, 5), 8);
