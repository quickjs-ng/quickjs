/*---
flags: [qjs:track-promise-rejections]
---*/

const promise = Promise.reject('reject')
const error = await Promise.resolve().then(() => promise).catch(e => e)
print('Got this error:', error)
