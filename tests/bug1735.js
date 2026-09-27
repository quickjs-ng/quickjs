// https://github.com/quickjs-ng/quickjs/issues/1735
import { assert, assertThrows } from "./assert.js";

// String.prototype.repeat throws a RangeError only when n < 0 or n is +Infinity.
assertThrows(RangeError, () => "".repeat(-1));
assertThrows(RangeError, () => "ab".repeat(-1));
assertThrows(RangeError, () => "".repeat(-Infinity));
assertThrows(RangeError, () => "".repeat(Infinity));
assertThrows(RangeError, () => "ab".repeat(Infinity));

// ToIntegerOrInfinity(NaN) is 0, and so is ToIntegerOrInfinity(undefined).
assert("".repeat(NaN), "");
assert("ab".repeat(NaN), "");
assert("ab".repeat(undefined), "");
assert("ab".repeat(-0), "");
assert("ab".repeat(0.9), "");
assert("ab".repeat(2.7), "abab");

assert("".repeat(0), "");
assert("".repeat(1), "");
assert("ab".repeat(0), "");
assert("ab".repeat(1), "ab");
assert("ab".repeat(3), "ababab");

// n copies of the empty string is the empty string, no matter how large n is.
assert("".repeat(Number.MAX_SAFE_INTEGER), "");
assert("".repeat(2147483648), "");
assert("".repeat(1e300), "");

// A non-empty string still overflows the maximum string length.
assertThrows(RangeError, () => "ab".repeat(Number.MAX_SAFE_INTEGER));
assertThrows(RangeError, () => "ab".repeat(1e300));
assertThrows(RangeError, () => "ab".repeat(2147483648));

// A finite count that is too large is a string length error, not a bad count.
function messageOf(f) {
    try {
        f();
    } catch (e) {
        return e.message;
    }
    return undefined;
}
assert(messageOf(() => "ab".repeat(2147483648)), "string too long");
assert(messageOf(() => "ab".repeat(-1)), "invalid repeat count");
assert(messageOf(() => "ab".repeat(Infinity)), "invalid repeat count");

// The receiver is coerced with ToString before the count is coerced.
var order = [];
var count = { valueOf() { order.push("count"); return 0 } };
var receiver = { toString() { order.push("receiver"); return "" } };
String.prototype.repeat.call(receiver, count);
assert(order.join(","), "receiver,count");
