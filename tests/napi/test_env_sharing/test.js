'use strict';

const storeEnv = napi(`./store_env.${ext}`);
const compareEnv = napi(`./compare_env.${ext}`);

assert.strictEqual(compareEnv(storeEnv), true,
                   'N-API environment pointers in two different modules have ' +
                   'the same value');
