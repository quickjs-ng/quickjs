/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
Array.prototype.lastIndexOf.call({length: 2**32-1}, 42)
