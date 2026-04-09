import { assert } from "./assert.js";

const arr = [{ x: 0 }, { x: 1 }];

delete arr[1];

assert(arr.length, 2);
assert(1 in arr, false);
assert(arr[1], undefined);

arr.push({ y: 2 });

assert(arr.length, 3);
assert(1 in arr, false);
assert(2 in arr, true);
assert(arr[1], undefined);
assert(arr[2].y, 2);
