/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
new Uint8Array(2**22).set({length: 2**22})
