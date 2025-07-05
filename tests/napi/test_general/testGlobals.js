'use strict';

const test_globals = await import(`test_general.${ext}`);

assert.strictEqual(test_globals.getUndefined(), undefined);
assert.strictEqual(test_globals.getNull(), null);
