import {assert, assertThrows} from "./assert.js"
let calls = 0
Error.prepareStackTrace = function() { calls++ }
function f() { f() }
assertThrows(RangeError, f)
assert(calls, 0)
