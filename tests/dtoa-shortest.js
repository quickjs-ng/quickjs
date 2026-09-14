// Shortest round-trip representation tests for the Ryu free-format dtoa
// path in dtoa.c. Expected strings are verified against V8 and the Ryu
// reference implementation. The denormal cases below are inputs where the
// previous bignum printer emitted a non-shortest 17-digit form; the fast
// path must emit the shortest form.
import { assert } from "./assert.js";

function u2d(u) {
    const buf = new ArrayBuffer(8);
    const dv = new DataView(buf);
    dv.setBigUint64(0, u);
    return dv.getFloat64(0);
}

const cases = [
    [0.1, "0.1"],
    [0.2, "0.2"],
    [0.3, "0.3"],
    [1.5, "1.5"],
    [-0.5, "-0.5"],
    [1e21, "1e+21"],
    [1e-7, "1e-7"],
    [0.000001, "0.000001"],
    [1e30, "1e+30"],
    [1e-30, "1e-30"],
    [Math.PI, "3.141592653589793"],
    [1.7976931348623157e308, "1.7976931348623157e+308"],
    [2.2250738585072014e-308, "2.2250738585072014e-308"],
    [Number.MIN_VALUE, "5e-324"],
    [1.0000000000000002, "1.0000000000000002"],
    [9007199254740992, "9007199254740992"],
    // bit-exact denormals where the shortest form has 16 digits
    [u2d(0x0060000000000000n), "7.120236347223045e-307"],
    [u2d(0x0031f57e09648c83n), "9.99e-308"],
    [u2d(0x0031fa182c40c60en), "1.0000000000000001e-307"],
    [u2d(0x0031feb24f1cff99n), "1.0010000000000002e-307"],
    [u2d(0x00666d1ce02a67b5n), "9.979999999999999e-307"],
    [u2d(0x006672dd8bbdafa3n), "9.99e-307"],
    [u2d(0x0066789e3750f790n), "9.999999999999999e-307"],
];

for (const [d, expected] of cases) {
    assert(String(d), expected);
    assert(d.toString(), expected);
}
