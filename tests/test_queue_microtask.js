function assert(actual, expected, message) {
    if (arguments.length == 1)
        expected = true;

    if (actual === expected)
        return;

    if (actual !== null && expected !== null
    &&  typeof actual == 'object' && typeof expected == 'object'
    &&  actual.toString() === expected.toString())
        return;

    throw Error("assertion failed: got |" + actual + "|" +
                ", expected |" + expected + "|" +
                (message ? " (" + message + ")" : ""));
}

function assert_throws(expected_error, func)
{
    var err = false;
    try {
        func();
    } catch(e) {
        err = true;
        if (!(e instanceof expected_error)) {
            throw Error("unexpected exception type");
        }
    }
    if (!err) {
        throw Error("expected exception");
    }
}

function assert_array_equals(a, b) {
    if (!Array.isArray(a) || !Array.isArray(b))
        return assert(false);

    assert(a.length === b.length);

    a.forEach((value, idx) => {
        assert(b[idx] === value);
    });
}

// load more elaborate version of assert if available
try { std.loadScript("test_assert.js"); } catch(e) {}

function test_types() {
  assert_throws(TypeError, () => queueMicrotask(), "no argument");
  assert_throws(TypeError, () => queueMicrotask(undefined), "undefined");
  assert_throws(TypeError, () => queueMicrotask(null), "null");
  assert_throws(TypeError, () => queueMicrotask(0), "0");
  assert_throws(TypeError, () => queueMicrotask({ handleEvent() { } }), "an event handler object");
  assert_throws(TypeError, () => queueMicrotask("window.x = 5;"), "a string");
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
    assert_array_equals(happenings, ["a", "b", "c"]);
  });
}

test_types();
test_async();
test_arguments();
test_async_order();
