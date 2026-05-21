import { assert } from "./assert.js";

function test_invalid_number_literal_location()
{
    let error;
    const source =
        "function fun() {\n" +
        "    let a = 123bcd;\n" +
        "}\n";

    try {
        eval(source);
    } catch (e) {
        error = e;
    }

    assert(error instanceof SyntaxError);
    assert(error.message, "invalid number literal");
    assert(error.stack.length >= 1);
    assert(error.stack[0].getFileName(), "<input>");
    assert(error.stack[0].getLineNumber(), 2);
    assert(error.stack[0].getColumnNumber(), 13);
}

Error.prepareStackTrace = (_, frames) => frames;
try {
    test_invalid_number_literal_location();
} finally {
    Error.prepareStackTrace = undefined;
}
