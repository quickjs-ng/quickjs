// https://github.com/quickjs-ng/quickjs/issues/1798
// Array spread must read [Symbol.iterator] exactly once.
import { assert, assertThrows } from "./assert.js";

{
    let gets = 0;
    const o = {
        get [Symbol.iterator]() {
            gets++;
            return function*() { yield 1; yield 2; yield 3; };
        },
    };
    const arr = [0, ...o, 4];
    assert(gets, 1);
    assert(arr.length, 5);
    assert(arr[0], 0);
    assert(arr[1], 1);
    assert(arr[2], 2);
    assert(arr[3], 3);
    assert(arr[4], 4);
}

{
    // Non-callable [Symbol.iterator] throws TypeError (not iterable).
    const bad = { [Symbol.iterator]: 42 };
    assertThrows(TypeError, () => { [...bad]; });
}

{
    // Missing [Symbol.iterator] throws TypeError.
    const plain = {};
    assertThrows(TypeError, () => { [...plain]; });
}

{
    // Fast-path: spreading an actual array still works.
    const src = [1, 2, 3];
    const out = [0, ...src, 4];
    assert(out.join(","), "0,1,2,3,4");
}
