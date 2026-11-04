/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
let s = '0,'
for (let i = 0; i < 22; i++) s += s
JSON.parse('[' + s + '0]')
