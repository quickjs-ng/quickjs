/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
Array.from({length: 2**17}, (_, i) => String(i)).sort()
