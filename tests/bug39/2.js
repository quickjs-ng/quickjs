/*---
flags: [qjs:track-promise-rejections]
---*/

const error = await Promise.resolve().then(() => Promise.reject('reject')).catch(e => e)
print('Got this error:', error)
