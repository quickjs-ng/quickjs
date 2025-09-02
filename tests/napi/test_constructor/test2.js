'use strict';

// Testing api calls for a constructor that defines properties
const TestConstructor = napi(`./test_constructor_name.${ext}`);
assert.strictEqual(TestConstructor.name, 'MyObject');
