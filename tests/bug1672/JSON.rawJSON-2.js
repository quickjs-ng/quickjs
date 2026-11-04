/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
// four files, one call apart, so that in one of them the interrupt lands in
// rawJSON whichever poll of the loop that is
function f() {}
f()
f()
for (;;) {
    try {
        JSON.rawJSON("1")
    } catch (e) {
        throw new Error("the interrupt became a " + e.name)
    }
}
