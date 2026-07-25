import { assert } from "./assert.js"
const ref = new WeakRef({})
const val = ref.deref() // should not throw
assert(typeof val, "object") // kept alive until the end of the job
