/*---
negative:
  phase: runtime
  type: Error
---*/
let finrec = new FinalizationRegistry(v => {})
let object = {object:"object"}
finrec.register(object, {held:"held"}, {token:"token"})
object = undefined
// abrupt termination should not leak |held|
// unfortunately only shows up in qjs, not run-test262,
// but still good to have a regression test
throw Error("ok")
