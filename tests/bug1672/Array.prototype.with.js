/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
Array.prototype.with.call({length: 2**22}, 0, 0)
