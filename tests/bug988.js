import {assert} from  "./assert.js"
const expected = [97,98,99]
const ab = new ArrayBuffer(0, {maxByteLength:3})
const dv = new DataView(ab)
ab.resize(3)
for (const [i,v] of Object.entries(expected)) dv.setUint8(i, v)
for (const [i,v] of Object.entries(expected)) assert(v, dv.getUint8(i))
