import { assert } from "./assert.js";

/* Array.prototype.sort writes every sorted value back with Set(..., true),
   even ones that are already in place. */

function throwsTypeError(f) {
    try {
        f();
    } catch (e) {
        return e instanceof TypeError;
    }
    return false;
}

/* a String's indices are read-only */
assert(throwsTypeError(() => Array.prototype.sort.call("aa")), true);
assert(throwsTypeError(() => Array.prototype.sort.call("ab")), true);

/* so are a frozen array's, even when it is already sorted */
assert(throwsTypeError(() => Object.freeze([1, 2]).sort()), true);

/* setters see every write */
{
    const writes = [];
    const o = { length: 2 };
    for (const i of [0, 1]) {
        let v = i;
        Object.defineProperty(o, i, {
            get: () => v,
            set: x => { writes.push(i); v = x; },
        });
    }
    Array.prototype.sort.call(o);
    assert(writes.join(), "0,1");
}
