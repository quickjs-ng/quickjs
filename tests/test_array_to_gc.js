import * as std from "qjs:std";
import { assert } from "./assert.js";

// The non-mutating array methods (with, toReversed, toSpliced, toSorted)
// allocate a fast array and then populate it by pulling values from the
// source via JS_TryGetPropertyInt64. When the source is an array-like with
// a property getter that triggers GC, the GC must see fully initialized
// slots — otherwise it would walk uninitialized JSValues in the newly
// allocated array. Run under ASAN to catch regressions.

function makeArrayLike(length, getterIndex) {
    const obj = { length };
    Object.defineProperty(obj, getterIndex, {
        configurable: true,
        get() {
            std.gc();
            return 1;
        },
    });
    return obj;
}

// with
{
    const obj = makeArrayLike(256, 0);
    Object.defineProperty(obj, 1, {
        value: 2,
        writable: true,
        configurable: true,
    });
    const res = Array.prototype.with.call(obj, 1, 9);
    assert(res.length, 256);
    assert(res[0], 1);
    assert(res[1], 9);
    assert(res[2], undefined);
    assert(res[255], undefined);
}

// toReversed
{
    const obj = makeArrayLike(256, 255);
    const res = Array.prototype.toReversed.call(obj);
    assert(res.length, 256);
    assert(res[0], 1);
    assert(res[1], undefined);
    assert(res[255], undefined);
}

// toSpliced
{
    const obj = makeArrayLike(256, 0);
    const res = Array.prototype.toSpliced.call(obj, 1, 0, 7);
    assert(res.length, 257);
    assert(res[0], 1);
    assert(res[1], 7);
    assert(res[2], undefined);
    assert(res[256], undefined);
}

// toSorted
{
    const obj = makeArrayLike(256, 0);
    const res = Array.prototype.toSorted.call(obj);
    assert(res.length, 256);
    // Sort places `undefined` at the end, so the single defined value (1) is first.
    assert(res[0], 1);
    assert(res[1], undefined);
    assert(res[255], undefined);
}
