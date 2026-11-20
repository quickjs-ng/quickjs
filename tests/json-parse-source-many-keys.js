import {assert} from "./assert.js"

// JSON.parse() with a reviver keeps the source text of the properties of an
// object in a hash table once there are more than eight. Integers are atoms
// as they are, so include keys that used to share a bucket of that table.
;(function() {
    const keys = []
    for (let i = 0; i < 300; i++)
        keys.push(String(i * 65536), "k" + i)
    const text = "{" + keys.map((k, i) => `"${k}":${i}.0`).join(",") + "}"
    const sources = new Map
    const obj = JSON.parse(text, function(key, value, context) {
        if (key !== "")
            sources.set(key, context.source)
        return value
    })
    assert(sources.size, keys.length)
    keys.forEach((k, i) => {
        assert(obj[k], i)
        assert(sources.get(k), i + ".0")
    })
})()
