/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
Array.prototype.findLastIndex.call({length: 2**32-1}, () => false)
