// Test ES2020 string export/import names
import { assert } from "./assert.js";
import * as mod from "./fixture_string_exports.js";

// Test string import names
import { "string-export-1" as str1 } from "./fixture_string_exports.js";
import { "string-export-2" as str2 } from "./fixture_string_exports.js";
import { "string-name" as strMixed } from "./fixture_string_exports.js";

// Test regular imports still work
import { regularExport, normalName } from "./fixture_string_exports.js";

// Verify values
assert(str1, "value-1");
assert(str2, "value-2");
assert(strMixed, "mixed-value");
assert(regularExport, "regular");
assert(normalName, "mixed-value");

// Verify module namespace has string-named exports
assert(mod["string-export-1"], "value-1");
assert(mod["string-export-2"], "value-2");
assert(mod["string-name"], "mixed-value");
assert(mod.regularExport, "regular");
assert(mod.normalName, "mixed-value");
