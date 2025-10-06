import {assert} from "./assert.js"

let ex
try {
    null.x
} catch (e) {
    ex = e
}
assert(ex instanceof TypeError)
assert(ex.message, "cannot read property 'x' of null")
ex = undefined

try {
    null["x"]
} catch (e) {
    ex = e
}
assert(ex instanceof TypeError)
assert(ex.message, "cannot read property 'x' of null")
ex = undefined

try {
    undefined.x
} catch (e) {
    ex = e
}
assert(ex instanceof TypeError)
assert(ex.message, "cannot read property 'x' of undefined")
ex = undefined

try {
    undefined["x"]
} catch (e) {
    ex = e
}
assert(ex instanceof TypeError)
assert(ex.message, "cannot read property 'x' of undefined")
ex = undefined
