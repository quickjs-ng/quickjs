/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
Array.prototype.flat.call({length: 2**32-1})
