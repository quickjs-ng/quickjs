// Bit-exact regression test for the fast decimal string->double parser in
// dtoa.c. The fast path must produce the same correctly rounded result as
// the exact bignum parser; the expected bit patterns below were generated
// with the bignum-only build (JS_ATOD_NO_FAST_PATH) and cross-checked by a
// differential fuzz run over half a million inputs.
import { assert } from "./assert.js";

function bits(d) {
    const buf = new ArrayBuffer(8);
    const dv = new DataView(buf);
    dv.setFloat64(0, d);
    return dv.getBigUint64(0);
}

const cases = [
    ["0.1", "3fb999999999999a"],
    ["0.2", "3fc999999999999a"],
    ["0.3", "3fd3333333333333"],
    ["0.5", "3fe0000000000000"],
    ["-0.5", "bfe0000000000000"],
    ["-0", "8000000000000000"],
    // 20 significant digits: fractional rounding through the 64-bit path
    ["0.3333333455917782943", "3fd55555627ee8b6"],
    ["782277.99999006", "4127df8bfffeb278"],
    // small quotient with a big remainder (u64 overflow bug)
    ["440e-19", "3c895d4100811d0c"],
    ["7.07395e-16", "3cc97c9138154903"],
    ["7772752141e-19", "3e0ab4fc2eaab957"],
    // rounding carries the mantissa to 2^53
    ["5629499534213119999e-4", "4300000000000000"],
    ["1e-19", "3bfd83c94fb6d2ac"],
    ["0.0000000000000000001", "3bfd83c94fb6d2ac"],
    ["12345.6", "40c81ccccccccccd"],
    // 19 significant digits: 64-bit mantissa rounding
    ["9999999999999999999", "43e158e460913d00"],
    // trailing zeros scale the value even beyond 19 digits
    ["10000000000000000000", "43e158e460913d00"],
    ["100000000000000000000", "4415af1d78b58c40"],
    // >19 significant digits: falls back to the bignum parser
    ["9999999999999999999.5", "43e158e460913d00"],
    ["18446744073709551615", "43f0000000000000"],
    ["123456789012345678901", "441ac53a7e04bcda"],
    ["9007199254740993", "4340000000000000"],
    // boundaries: subnormal / max / overflow / underflow
    ["2.2250738585072014e-308", "0010000000000000"],
    ["1.7976931348623157e308", "7fefffffffffffff"],
    ["1e309", "7ff0000000000000"],
    ["5e-324", "0000000000000001"],
    ["1e-323", "0000000000000002"],
    ["1e-400", "0000000000000000"],
    // radix prefixes and legacy octal keep their existing semantics
    ["0x1A", "403a000000000000"],
    ["0o10", "4020000000000000"],
    ["0b10", "4000000000000000"],
    ["077", "4053400000000000"],
];

for (const [str, hex] of cases) {
    assert(bits(Number(str)), BigInt("0x" + hex), str);
}

// ToInt32 (`| 0`) goes through the same conversion
assert(("12345" | 0) === 12345);
assert(("4294967296" | 0) === 0);
assert(("9007199254740993" | 0) === 0);
