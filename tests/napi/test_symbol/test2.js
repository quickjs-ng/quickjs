'use strict';

// testing api calls for symbol
const test_symbol = napi(`./test_symbol.${ext}`);

const fooSym = test_symbol.New('foo');
const myObj = {};
myObj.foo = 'bar';
myObj[fooSym] = 'baz';
Object.keys(myObj); // -> [ 'foo' ]
Object.getOwnPropertyNames(myObj); // -> [ 'foo' ]
Object.getOwnPropertySymbols(myObj); // -> [ Symbol(foo) ]
assert.strictEqual(Object.getOwnPropertySymbols(myObj)[0], fooSym);
