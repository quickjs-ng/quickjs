'use strict';

const storeEnv = await import(`store_env.${ext}`);
const compareEnv = await import(`compare_env.${ext}`);

assert.strictEqual(compareEnv(storeEnv), true,
                   'N-API environment pointers in two different modules have ' +
                   'the same value');
