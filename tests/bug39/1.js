/*---
flags: [qjs:track-promise-rejections]
---*/

Promise.reject().catch(() => print('oops'))
Promise.resolve().then(() => print('ok'))
