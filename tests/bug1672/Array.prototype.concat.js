/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
[].concat({length: 2**32-1, [Symbol.isConcatSpreadable]: true})
