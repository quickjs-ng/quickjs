'use strict';
// Flags: --expose-gc

const binding = napi(`./binding.${ext}`);

assert.strictEqual(binding.newBuffer().toString(), binding.theText);
assert.strictEqual(binding.newExternalBuffer().toString(), binding.theText);
console.log('gc1');
global.gc();
assert.strictEqual(binding.getDeleterCallCount(), 1);
assert.strictEqual(binding.copyBuffer().toString(), binding.theText);

let buffer = binding.staticBuffer();
assert.strictEqual(binding.bufferHasInstance(buffer), true);
assert.strictEqual(binding.bufferInfo(buffer), true);
buffer = null;
global.gc();
console.log('gc2');
assert.strictEqual(binding.getDeleterCallCount(), 2);
