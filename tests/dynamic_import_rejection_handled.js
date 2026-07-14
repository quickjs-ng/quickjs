/*---
flags: [qjs:track-promise-rejections]
---*/

// import() of a throwing module must not leak the module's internal rejection.
const error = await import("./fixture_throwing_module.js").then(() => 'ok', e => e)
print('Got this error:', error.message)
