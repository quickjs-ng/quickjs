import {assert} from "./assert.js"
import {f} from "./fixture_cyclic_import.js"
export {f}
export function g(x) { return x + 1 }
assert(f(1), 4)
