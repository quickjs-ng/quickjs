import { assert, assertThrows } from "./assert.js";

// Test ClassSetReservedPunctuators: &, -, !, #, %, ,, :, ;, <, =, >, @, `, and ~
assert(new RegExp("[\\q{\\-}]", "v").test("-"), true);
assert(new RegExp("[\\q{\\&}]", "v").test("&"), true);
assert(new RegExp("[\\q{\\!}]", "v").test("!"), true);
assert(new RegExp("[\\q{\\#}]", "v").test("#"), true);
assert(new RegExp("[\\q{\\%}]", "v").test("%"), true);
assert(new RegExp("[\\q{\\,}]", "v").test(","), true);
assert(new RegExp("[\\q{\\:}]", "v").test(":"), true);
assert(new RegExp("[\\q{\\;}]", "v").test(";"), true);
assert(new RegExp("[\\q{\\<}]", "v").test("<"), true);
assert(new RegExp("[\\q{\\=}]", "v").test("="), true);
assert(new RegExp("[\\q{\\>}]", "v").test(">"), true);
assert(new RegExp("[\\q{\\@}]", "v").test("@"), true);
assert(new RegExp("[\\q{\\`}]", "v").test("`"), true);
assert(new RegExp("[\\q{\\~}]", "v").test("~"), true);

// Also test negative cases
assertThrows(SyntaxError, () => new RegExp("\\-", "v").test("-"));
assertThrows(SyntaxError, () => new RegExp("\\%", "v").test("%"));
assertThrows(SyntaxError, () => new RegExp("[\\&]", "u").test("&"));