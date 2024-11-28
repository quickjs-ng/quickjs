/*---
negative:
  phase: parse
  type: SyntaxError
---*/
// the import statement makes it a module but `await = 42` is a SyntaxError
import * as _ from "dummy"
await = 42
