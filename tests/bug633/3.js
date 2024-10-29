/*---
flags: [qjs:no-detect-module]
---*/
;(function() { const undefined = 42 })() // not a SyntaxError, not at global scope
