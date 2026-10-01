/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
const a = new Array(1e5).fill(0)
for (;;) a.includes(42)
