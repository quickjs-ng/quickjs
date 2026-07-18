import { assert } from "./assert.js";

/* A re-export of a missing binding must blame the *source* module and the
   name looked up there -- exactly like a direct import of the same missing
   binding does. Comparing the two messages avoids depending on the module
   path surviving intact in the error: long absolute paths get truncated in
   the fixed-size atom buffer, so a substring check on the filename is
   unreliable in CI. */
const reexportErr = await import("./fixture_reexport_missing.js").then(() => null, e => e);
const directErr = await import("./fixture_reexport_direct.js").then(() => null, e => e);

assert(reexportErr instanceof SyntaxError);
assert(directErr instanceof SyntaxError);
assert(reexportErr.message, directErr.message);
