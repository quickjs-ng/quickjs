'use strict';
const addon = napi(`./binding.${ext}`);

addon.RunCallback(function(msg) {
  assert.strictEqual(msg, 'hello world');
});

function testRecv(desiredRecv) {
  addon.RunCallbackWithRecv(function() {
    assert.strictEqual(this, desiredRecv);
  }, desiredRecv);
}

testRecv(undefined);
testRecv(null);
testRecv(5);
testRecv(true);
testRecv('Hello');
testRecv([]);
testRecv({});
