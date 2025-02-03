import {assert} from "./assert.js";

Error.stackTraceLimit = Infinity;
assert(Error.stackTraceLimit === Infinity);
assert(new Error("error").stack.includes("bug858.js")); // stack should not be empty

Error.stackTraceLimit = -Infinity;
assert(Error.stackTraceLimit === -Infinity);
assert(!new Error("error").stack.includes("bug858.js")); // stack should be empty

Error.stackTraceLimit = NaN;
assert(Number.isNaN(Error.stackTraceLimit));
assert(!new Error("error").stack.includes("bug858.js")); // stack should be empty

Error.stackTraceLimit = -0;
assert(Object.is(Error.stackTraceLimit, -0));
assert(!new Error("error").stack.includes("bug858.js")); // stack should be empty

const obj = { valueOf() { throw "evil" } };
Error.stackTraceLimit = obj;
assert(Error.stackTraceLimit === obj);
try {
  throw new Error("fail")
} catch (e) {
  assert(e.message.includes("fail"));
}
