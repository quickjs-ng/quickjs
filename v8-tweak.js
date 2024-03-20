;(function() {
    "use strict"
    const print = globalThis.print
    globalThis.print = function() {}    // print nothing, v8 tests are chatty
    let count = 0                       // rate limit to avoid excessive logs
    globalThis.failWithMessage = function(message) {
        if (count > 99) return
        if (++count > 99) return print("<output elided>")
        print(String(message).slice(0, 300))
    }
    globalThis.formatFailureText = function(expectedText, found, name_opt) {
        var message = "Fail" + "ure"
        if (name_opt) {
            // Fix this when we ditch the old test runner.
            message += " (" + name_opt + ")"
        }
        var foundText = prettyPrinted(found)
        if (expectedText.length <= 60 && foundText.length <= 60) {
            message += ": expected <" + expectedText + "> found <" + foundText + ">"
        } else {
            message += ":\nexpected:\n" + expectedText + "\nfound:\n" + foundText
        }
        return message
    }
})()
