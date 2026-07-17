import { assert } from "./assert.js";

const err = await import("./fixture_reexport_missing.js").then(() => null, e => e);
assert(err !== null);
assert(err instanceof SyntaxError);
/* DEBUG: surface the actual message on failure */
assert(err.message.includes("fixture_reexport_source"), true, "ACTUAL=[" + err.message + "]");
assert(!err.message.includes("fixture_reexport_missing"), true, "ACTUAL=[" + err.message + "]");
