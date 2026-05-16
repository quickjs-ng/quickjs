import { assert } from "./assert.js";

function closingIterable(stacks)
{
    return {
        [Symbol.iterator]() {
            return {
                next() {
                    return { done: false, value: 1 };
                },
                return() {
                    stacks.push(new Error().stack);
                    return { done: true };
                },
            };
        },
    };
}

function assertCallerLine(frames, expectedLine, message)
{
    assert(frames.length >= 2, true, message);
    assert(frames[1].getFileName().endsWith("test_for_of_line_numbers.js"), true, message);
    assert(frames[1].getLineNumber(), expectedLine, message);
}

function test_for_of_empty_body_line_number()
{
    const stacks = [];
    const expectedLine = 31;
    for (const _ of closingIterable(stacks)) break;
    assertCallerLine(stacks[0], expectedLine, "for-of iterator close");
}

function test_for_of_destructuring_line_number()
{
    const stacks = [];
    const outer = [closingIterable(stacks)];
    const expectedLine = 40;
    for (const [_] of outer) break;
    assertCallerLine(stacks[0], expectedLine, "destructuring iterator close");
}

Error.prepareStackTrace = (_, frames) => frames;
try {
    test_for_of_empty_body_line_number();
    test_for_of_destructuring_line_number();
} finally {
    Error.prepareStackTrace = undefined;
}
