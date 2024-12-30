/*---
negative:
  phase: runtime
  type: RangeError
---*/
function f() { f.apply(null) } // was problematic under ASan
f()
