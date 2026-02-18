/*---
negative:
  phase: runtime
  type: SyntaxError
---*/
new RegExp("[", "v") // run under ASAN
