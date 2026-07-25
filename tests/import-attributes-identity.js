import { assert } from "./assert.js";

/* Module identity is keyed by (specifier, import attributes): the same
   file can be loaded as different module types, equal attribute sets
   share one instance, and this module can import itself as text. */

import textDefault from "./fixture_import_attributes.json" with { type: "text" };
import jsonDefault from "./fixture_import_attributes.json" with { type: "json" };
import jsonAgain from "./fixture_import_attributes.json" with { type: "json" };
import self from "./import-attributes-identity.js" with { type: "text" };

/* same file, different types, distinct modules */
assert(typeof textDefault, "string");
assert(JSON.parse(textDefault).answer, 42);
assert(typeof jsonDefault, "object");
assert(jsonDefault.answer, 42);

/* equal attributes resolve to the same module instance */
assert(jsonAgain === jsonDefault, true);

/* a JS module can import its own source as text */
assert(typeof self, "string");
assert(self.includes("import-attributes-identity"), true);

/* dynamic imports share identity with static imports */
const viaJson = await import("./fixture_import_attributes.json", { with: { type: "json" } });
assert(viaJson.default === jsonDefault, true);
const viaText = await import("./fixture_import_attributes.json", { with: { type: "text" } });
assert(viaText.default, textDefault);

/* no attributes and .json suffix still defaults to JSON, as another
   distinct module */
const plain = await import("./fixture_import_attributes.json");
assert(typeof plain.default, "object");
assert(plain.default === jsonDefault, false);
