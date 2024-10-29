/*---
flags: [qjs:no-detect-module]
negative:
  phase: parse
  type: SyntaxError
---*/
const undefined = 42 // SyntaxError at global scope
