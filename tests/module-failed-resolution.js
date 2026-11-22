import {assert} from "./assert.js"

// fixture a imports fixture b and a module that does not exist; b imports a.
// b is resolved while a is being resolved, and records a pointer to a. When
// a then fails to resolve it is freed, and b used to stay loaded, marked as
// resolved, with that pointer.

async function load(name) {
    try {
        await import(name)
        return "loaded"
    } catch (e) {
        return "failed"
    }
}

assert(await load("./fixture_failed_resolution_a.js"), "failed")
// used to read the freed definition of a
assert(await load("./fixture_failed_resolution_b.js"), "failed")
assert(await load("./fixture_failed_resolution_a.js"), "failed")
assert(await load("./fixture_failed_resolution_b.js"), "failed")
