'use strict';

// testing api calls for function
const test_function = napi(`./test_function.${ext}`);


function func1() {
  return 1;
}
assert.strictEqual(test_function.TestCall(func1), 1);

function func2() {
  console.log('hello world!');
  return null;
}
assert.strictEqual(test_function.TestCall(func2), null);

function func3(input) {
  return input + 1;
}
assert.strictEqual(test_function.TestCall(func3, 1), 2);

function func4(input) {
  return func3(input);
}
assert.strictEqual(test_function.TestCall(func4, 1), 2);

assert.strictEqual(test_function.TestName.name, 'Name');
assert.strictEqual(test_function.TestNameShort.name, 'Name_');
