/*---
flags: [qjs:set-interrupt-handler]
negative:
  phase: runtime
  type: InternalError
---*/
class A extends Uint8Array { static get [Symbol.species]() { return Uint16Array } }
new A(2**22).slice()
