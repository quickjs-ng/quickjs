;(function() {
    "use strict"
    const print = globalThis.print
    globalThis.print = function() {}    // print nothing, v8 tests are chatty
    let count = 0                       // rate limit to avoid excessive logs
    globalThis.failWithMessage = function(message) {
        if (count > 99) return
        if (++count > 99) return print("<output elided>")
        print(String(message).slice(0, 128))
    }
})()
