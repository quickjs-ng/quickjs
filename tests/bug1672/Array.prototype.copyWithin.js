/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
Array.prototype.copyWithin.call({length: 2**32-1}, 0, 1)
