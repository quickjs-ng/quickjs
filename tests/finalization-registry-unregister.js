import {assert} from "./assert.js"

// unregister() frees the held values of the entries it removes. A held value
// can be the target of another entry of the same registry; freeing it then
// removes that entry too, while unregister() is still walking the list.

{
    const registry = new FinalizationRegistry(() => {})
    const token = {}
    const first = {}
    let second = {}
    registry.register(first, second, token) // held value: second
    registry.register(second, 42, token)    // second is also a target
    second = undefined                      // now only the first entry keeps it alive
    assert(registry.unregister(token), true)
    assert(registry.unregister(token), false)
}

{
    // a chain of held values that are each the next entry's target
    const registry = new FinalizationRegistry(() => {})
    const token = {}
    const first = {}
    let a = {}, b = {}, c = Symbol("c")
    registry.register(first, a, token)
    registry.register(a, b, token)
    registry.register(b, c, token)
    registry.register(c, 42, token)
    a = b = c = undefined
    assert(registry.unregister(token), true)
    assert(registry.unregister(token), false)
}

{
    // the entry that goes away was registered with a different token
    const registry = new FinalizationRegistry(() => {})
    const token = {}, other = {}
    const first = {}
    let second = {}
    registry.register(first, second, token)
    registry.register(second, 42, other)
    second = undefined
    assert(registry.unregister(token), true)
    assert(registry.unregister(other), false)
    const third = {}
    registry.register(third, 42, other)
    assert(registry.unregister(other), true)
}
