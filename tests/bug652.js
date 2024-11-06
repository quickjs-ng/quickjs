import { assert } from "./assert.js"
const ref = new WeakRef({})
const val = ref.deref() // should not throw
assert(val, undefined)
