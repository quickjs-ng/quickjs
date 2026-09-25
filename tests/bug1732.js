import { assert } from "./assert.js";
const f = (function* () {}).constructor("aa").bind()
assert(Object.prototype.toString.call(f) === "[object GeneratorFunction]", true);