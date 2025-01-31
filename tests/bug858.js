import {assert} from "./assert.js";

Error.stackTraceLimit = Infinity;
assert(new Error("error").stack.includes("bug858.js")); // stack should not be empty

Error.stackTraceLimit = -Infinity;
assert(!new Error("error").stack.includes("bug858.js")); // stack should be empty

Error.stackTraceLimit = NaN;
assert(!new Error("error").stack.includes("bug858.js")); // stack should be empty