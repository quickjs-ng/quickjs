import { assert, assertThrows } from "./assert.js";

function test_symbol_iterator_reentry() {
  let it;
  let calls = 0;
  const evil = {
    [Symbol.iterator]() {
      calls++;
      assertThrows(TypeError, () => it.return());
      return [][Symbol.iterator]();
    },
  };

  it = Iterator.concat(evil);
  const result = it.next();
  assert(calls, 1);
  assert(result.done, true);
  assert(result.value, undefined);
}

function test_next_getter_reentry() {
  let it;
  let calls = 0;
  const evil = {
    [Symbol.iterator]() {
      return {
        get next() {
          calls++;
          assertThrows(TypeError, () => it.return());
          return () => ({ done: true, value: 1 });
        },
      };
    },
  };

  it = Iterator.concat(evil);
  const result = it.next();
  assert(calls, 1);
  assert(result.done, true);
  assert(result.value, undefined);
}

function test_value_getter_reentry() {
  let it;
  let calls = 0;
  const evil = {
    [Symbol.iterator]() {
      return {
        next() {
          return {
            done: false,
            get value() {
              calls++;
              assertThrows(TypeError, () => it.return());
              return 1;
            },
          };
        },
      };
    },
  };

  it = Iterator.concat(evil);
  const result = it.next();
  assert(calls, 1);
  assert(result.done, false);
  assert(result.value, 1);
}

test_symbol_iterator_reentry();
test_next_getter_reentry();
test_value_getter_reentry();
