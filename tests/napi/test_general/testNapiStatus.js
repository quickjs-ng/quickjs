'use strict';

const addon = napi(`./test_general.${ext}`);

addon.createNapiError();
assert(addon.testNapiErrorCleanup(), 'napi_status cleaned up for second call');
