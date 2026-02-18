// run under ASAN
let key = Symbol("fortytwo")
let wm = new WeakMap()
let fr = new FinalizationRegistry(() => {})
fr.register(key, 42)
wm.set(key, fr)
fr = null
key = null
