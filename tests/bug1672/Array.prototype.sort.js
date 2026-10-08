/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
Array.prototype.sort.call({length: 2**32-1})
