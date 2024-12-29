/*---
negative:
  phase: runtime
  type: RangeError
---*/
function f() { f() } // was problematic under ASan
f()
