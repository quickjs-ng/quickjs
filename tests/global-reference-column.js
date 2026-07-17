import { assert } from "./assert.js";

/* A ReferenceError for an undefined global should point at the column of the
   reference, not column 1 or the enclosing function. The location is exposed
   through the error's stack trace. */
function refColumn(src) {
    try {
        (0, eval)(src);
    } catch (e) {
        const m = /<input>:(\d+):(\d+)/.exec(e.stack);
        if (m)
            return +m[2];
    }
    return -1;
}

assert(refColumn("x"), 1);
assert(refColumn("   missingGlobal"), 4);
assert(refColumn("let a=1; alsoMissing"), 10);
