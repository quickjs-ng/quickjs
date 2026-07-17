import { assert } from "./assert.js";

/* A SyntaxError should report the column of the offending token, not always
   column 1. The location is exposed through the error's stack trace. */
function errorLocation(src) {
    try {
        eval(src);
    } catch (e) {
        const m = /<input>:(\d+):(\d+)/.exec(e.stack);
        if (m)
            return { line: +m[1], col: +m[2] };
    }
    return { line: -1, col: -1 };
}

assert(errorLocation("let x = ;").col, 9);
assert(errorLocation("1 + + ;").col, 7);
assert(errorLocation("hocuspocus(").col, 12);
assert(errorLocation("a b").col, 3);

/* line number stays correct and the column is relative to the line start */
{
    const loc = errorLocation("let ok = 1\nlet z = @");
    assert(loc.line, 2);
    assert(loc.col, 9);
}
