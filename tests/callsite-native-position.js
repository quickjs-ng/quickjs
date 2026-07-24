import { assert } from "./assert.js";

/* CallSite position getters return null for frames with no source position
   (native frames), matching V8, instead of leaking the internal -1. */

Error.prepareStackTrace = (_, frames) => frames;
const frames = [1].map(function m() { return new Error().stack; })[0];
Error.prepareStackTrace = undefined;

const native = frames.find((f) => f.isNative());
assert(native !== undefined, true);
assert(native.getLineNumber(), null);
assert(native.getColumnNumber(), null);
assert(native.getFileName(), null);

/* frames with a real position still report numbers */
const scripted = frames.find((f) => !f.isNative());
assert(typeof scripted.getLineNumber(), "number");
assert(typeof scripted.getColumnNumber(), "number");
