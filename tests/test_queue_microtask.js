import { assert, assertArrayEquals, assertThrows } from "./assert.js";

function test_types() {
  assertThrows(TypeError, () => queueMicrotask(), "no argument");
  assertThrows(TypeError, () => queueMicrotask(undefined), "undefined");
  assertThrows(TypeError, () => queueMicrotask(null), "null");
  assertThrows(TypeError, () => queueMicrotask(0), "0");
  assertThrows(TypeError, () => queueMicrotask({ handleEvent() { } }), "an event handler object");
  assertThrows(TypeError, () => queueMicrotask("window.x = 5;"), "a string");
}

function test_async() {
  let called = false;
  queueMicrotask(() => {
    called = true;
  });
  assert(!called);
}

function test_arguments() {
  queueMicrotask(function () { // note: intentionally not an arrow function
    assert(arguments.length === 0);
  }, "x", "y");
};

function test_async_order() {
  const happenings = [];
  Promise.resolve().then(() => happenings.push("a"));
  queueMicrotask(() => happenings.push("b"));
  Promise.reject().catch(() => happenings.push("c"));
  queueMicrotask(() => {
    assertArrayEquals(happenings, ["a", "b", "c"]);
  });
}

test_types();
test_async();
test_arguments();
test_async_order();
