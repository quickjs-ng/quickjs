import { assert, assertThrows } from "./assert.js";

function pat(plain) {
    return "(?<g>a)" + "|x".repeat(plain) + "|(?<g>c)";
}

function messageOf(re) {
    try {
        new RegExp(re);
    } catch (e) {
        return e.message;
    }
    return undefined;
}

// 256 alternatives: fine.
assert(new RegExp(pat(254)).test("a"), true);

// 257 alternatives and beyond: explicit error instead of a wrong
// "duplicate group name" report.
assert(messageOf(pat(255)), "too many named groups");
assert(messageOf(pat(1000)), "too many named groups");

// A genuine duplicate within the same alternative must still be rejected.
assert(messageOf("(?<g>a)(?<g>b)"), "duplicate group name");
assertThrows(SyntaxError, () => new RegExp("(?<g>a)(?<g>b)"));
