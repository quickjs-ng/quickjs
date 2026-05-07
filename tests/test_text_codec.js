import { assert, assertThrows, assertArrayEquals } from "./assert.js";

function bytes(arr) { return new Uint8Array(arr); }
function arr(u8) { return Array.from(u8); }

function test_encoder_basic() {
    const e = new TextEncoder();
    assert(e.encoding, "utf-8");
    assert(Object.prototype.toString.call(e), "[object TextEncoder]");

    assertArrayEquals(arr(e.encode()), []);
    assertArrayEquals(arr(e.encode(undefined)), []);
    assertArrayEquals(arr(e.encode("")), []);
    assertArrayEquals(arr(e.encode("hi")), [0x68, 0x69]);
    // U+2603 SNOWMAN — 3-byte sequence.
    assertArrayEquals(arr(e.encode("☃")), [0xE2, 0x98, 0x83]);
    // U+10000 via surrogate pair — 4-byte sequence.
    assertArrayEquals(arr(e.encode("𐀀")), [0xF0, 0x90, 0x80, 0x80]);
    // ToString coercion.
    assertArrayEquals(arr(e.encode(null)), [0x6E, 0x75, 0x6C, 0x6C]); // "null"
    assertArrayEquals(arr(e.encode(42)), [0x34, 0x32]);                // "42"
}

function test_encoder_lone_surrogates() {
    // USVString conversion: lone surrogates become U+FFFD before encoding.
    const e = new TextEncoder();
    assertArrayEquals(arr(e.encode("\uD800")), [0xEF, 0xBF, 0xBD]);
    assertArrayEquals(arr(e.encode("\uDFFF")), [0xEF, 0xBF, 0xBD]);
    assertArrayEquals(arr(e.encode("\uDC00")), [0xEF, 0xBF, 0xBD]);
    assertArrayEquals(arr(e.encode("a\uD800b")),
                      [0x61, 0xEF, 0xBF, 0xBD, 0x62]);
    // Two adjacent lone high surrogates: each replaced independently.
    assertArrayEquals(arr(e.encode("\uD800\uD800")),
                      [0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD]);
    // Reverse-order surrogates (low then high): both lone.
    assertArrayEquals(arr(e.encode("\uDC00\uD800")),
                      [0xEF, 0xBF, 0xBD, 0xEF, 0xBF, 0xBD]);
    // Lone high followed by ASCII before a matched pair: only the lone one
    // is replaced.
    assertArrayEquals(arr(e.encode("\uD800a😀")),
                      [0xEF, 0xBF, 0xBD, 0x61, 0xF0, 0x9F, 0x98, 0x80]);
}

function test_encode_into_basic() {
    const e = new TextEncoder();

    let dst = new Uint8Array(8);
    let r = e.encodeInto("hi", dst);
    assert(r.read, 2);
    assert(r.written, 2);
    assertArrayEquals(arr(dst.subarray(0, 2)), [0x68, 0x69]);

    // Surrogate pair: read counts UTF-16 code units (2), written is 4 bytes.
    dst = new Uint8Array(8);
    r = e.encodeInto("😀", dst);
    assert(r.read, 2);
    assert(r.written, 4);
    assertArrayEquals(arr(dst.subarray(0, 4)), [0xF0, 0x9F, 0x98, 0x80]);

    // Lone surrogate replaced with U+FFFD; read still counts 1 UTF-16 unit.
    dst = new Uint8Array(8);
    r = e.encodeInto("a\uD800", dst);
    assert(r.read, 2);
    assert(r.written, 4);
    assertArrayEquals(arr(dst.subarray(0, 4)), [0x61, 0xEF, 0xBF, 0xBD]);

    // Empty source.
    dst = new Uint8Array(4); dst.fill(0xAA);
    r = e.encodeInto("", dst);
    assert(r.read, 0); assert(r.written, 0);
    assertArrayEquals(arr(dst), [0xAA, 0xAA, 0xAA, 0xAA]);

    // Empty destination.
    r = e.encodeInto("abc", new Uint8Array(0));
    assert(r.read, 0); assert(r.written, 0);
}

function test_encode_into_partial() {
    const e = new TextEncoder();

    // Destination too small for the next char's full encoding — must NOT
    // write a partial sequence.
    let dst = new Uint8Array(2); dst.fill(0xAA);
    let r = e.encodeInto("☃hi", dst);   // snowman is 3 bytes
    assert(r.read, 0); assert(r.written, 0);
    assertArrayEquals(arr(dst), [0xAA, 0xAA]);

    // Same for U+FFFD replacement of a lone surrogate (3 bytes).
    dst = new Uint8Array(2); dst.fill(0xAA);
    r = e.encodeInto("\uD800X", dst);
    assert(r.read, 0); assert(r.written, 0);
    assertArrayEquals(arr(dst), [0xAA, 0xAA]);

    // Some chars fit, then we stop short of an over-large one.
    dst = new Uint8Array(4); dst.fill(0xAA);
    r = e.encodeInto("ab☃c", dst);
    assert(r.read, 2); assert(r.written, 2);
    assertArrayEquals(arr(dst), [0x61, 0x62, 0xAA, 0xAA]);
}

function test_encode_into_argument_errors() {
    const e = new TextEncoder();

    assertThrows(TypeError, () => e.encodeInto());
    assertThrows(TypeError, () => e.encodeInto("x"));
    assertThrows(TypeError, () => e.encodeInto("x", "not a buffer"));
    assertThrows(TypeError, () => e.encodeInto("x", new Int8Array(4)));
    assertThrows(TypeError, () => e.encodeInto("x", new Uint16Array(4)));
    assertThrows(TypeError, () => e.encodeInto("x", new Uint8ClampedArray(4)));
    assertThrows(TypeError, () => e.encodeInto("x", new ArrayBuffer(4)));

    // Source is stringified before destination is validated (spec order).
    let calls = [];
    const src = { toString() { calls.push("src"); return "x"; } };
    assertThrows(TypeError, () => e.encodeInto(src, "not a buffer"));
    assertArrayEquals(calls, ["src"]);
}

function test_encoder_brand() {
    assertThrows(TypeError, () => TextEncoder.prototype.encode.call({}, "x"));
    assertThrows(TypeError, () =>
        TextEncoder.prototype.encodeInto.call({}, "x", new Uint8Array(4)));
    // Calling the constructor without `new`.
    assertThrows(TypeError, () => TextEncoder());
}

function test_decoder_basic() {
    const d = new TextDecoder();
    assert(d.encoding, "utf-8");
    assert(d.fatal, false);
    assert(d.ignoreBOM, false);
    assert(Object.prototype.toString.call(d), "[object TextDecoder]");

    assert(d.decode(), "");
    assert(d.decode(undefined), "");
    assert(d.decode(bytes([])), "");
    assert(d.decode(bytes([0x68, 0x69])), "hi");
    assert(d.decode(bytes([0xE2, 0x98, 0x83])), "☃");
    assert(d.decode(bytes([0xF0, 0x9F, 0x98, 0x80])), "😀"); // U+1F600
}

function test_decoder_input_types() {
    const d = new TextDecoder();
    const data = [0x61, 0x62, 0x63];

    assert(d.decode(new Uint8Array(data)), "abc");
    assert(d.decode(new Uint8Array(data).buffer), "abc");
    assert(d.decode(new Int8Array(new Uint8Array(data).buffer)), "abc");

    // Subarray view at an offset must use that view's bytes only.
    const big = new Uint8Array([0xFF, 0x61, 0x62, 0x63, 0xFF]);
    assert(d.decode(big.subarray(1, 4)), "abc");

    assertThrows(TypeError, () => d.decode("not a buffer"));
    assertThrows(TypeError, () => d.decode({}));
    assertThrows(TypeError, () => d.decode(null));
    assertThrows(TypeError, () => d.decode(123));
}

function test_decoder_label() {
    for (const label of [
        "utf-8", "UTF-8", "utf8", "UTF8", "Utf-8",
        "  utf-8\t", "\nutf-8\r\f", "\fUTF-8 ",
        "unicode-1-1-utf-8", "unicode11utf8",
        "unicode20utf8", "x-unicode20utf8",
    ]) {
        assert(new TextDecoder(label).encoding, "utf-8");
    }
    for (const label of ["latin1", "iso-8859-1", "utf-16", "windows-1252",
                         "utf-7", "ascii", ""]) {
        assertThrows(RangeError, () => new TextDecoder(label));
    }
}

function test_decoder_options() {
    let d = new TextDecoder("utf-8", { fatal: true });
    assert(d.fatal, true); assert(d.ignoreBOM, false);

    d = new TextDecoder("utf-8", { ignoreBOM: true });
    assert(d.fatal, false); assert(d.ignoreBOM, true);

    d = new TextDecoder("utf-8", { fatal: true, ignoreBOM: true });
    assert(d.fatal, true); assert(d.ignoreBOM, true);

    // Truthy/falsy coercion.
    d = new TextDecoder("utf-8", { fatal: 1, ignoreBOM: 0 });
    assert(d.fatal, true); assert(d.ignoreBOM, false);

    // Missing or non-object options: defaults.
    d = new TextDecoder("utf-8");
    assert(d.fatal, false); assert(d.ignoreBOM, false);
}

function test_decoder_bom() {
    const bom = [0xEF, 0xBB, 0xBF];

    // Default: BOM at start is stripped.
    let d = new TextDecoder();
    assert(d.decode(bytes([...bom, 0x68, 0x69])), "hi");
    // BOM in the middle is kept as U+FEFF.
    assert(d.decode(bytes([0x68, ...bom, 0x69])), "h﻿i");
    // ignoreBOM=true: BOM is kept.
    d = new TextDecoder("utf-8", { ignoreBOM: true });
    assert(d.decode(bytes([...bom, 0x68])), "﻿h");
    // Decoder state is reset on non-stream call: a fresh BOM is honored.
    d = new TextDecoder();
    assert(d.decode(bytes([...bom, 0x61])), "a");
    assert(d.decode(bytes([...bom, 0x62])), "b");
    // BOM split across stream calls is still recognized.
    d = new TextDecoder();
    assert(d.decode(bytes([0xEF, 0xBB]), { stream: true }), "");
    assert(d.decode(bytes([0xBF, 0x68])), "h");
}

function test_decoder_invalid_sequences() {
    const d = new TextDecoder();

    // Stray continuation byte.
    assert(d.decode(bytes([0x80])), "�");

    // Lead byte followed by an out-of-range continuation: emit U+FFFD AND
    // re-process the offending byte.
    assert(d.decode(bytes([0xE0, 0x41])), "�A");
    assert(d.decode(bytes([0xE0, 0x80])), "��");
    assert(d.decode(bytes([0xF0, 0x80])), "��");
    assert(d.decode(bytes([0xF4, 0x90])), "��");
    assert(d.decode(bytes([0xF0, 0x90, 0x7F])), "�");

    // Truly partial sequences (valid prefix, no following byte): single U+FFFD.
    assert(d.decode(bytes([0xE0])), "�");
    assert(d.decode(bytes([0xE0, 0xA0])), "�");
    assert(d.decode(bytes([0xF0, 0x90])), "�");
    assert(d.decode(bytes([0xF0, 0x90, 0x80])), "�");

    // Bytes that can never start a UTF-8 sequence.
    assert(d.decode(bytes([0xC0])), "�");
    assert(d.decode(bytes([0xC1])), "�");
    assert(d.decode(bytes([0xF5])), "�");
    assert(d.decode(bytes([0xFF])), "�");
}

function test_decoder_fatal() {
    const d = new TextDecoder("utf-8", { fatal: true });
    assert(d.decode(bytes([0x68, 0x69])), "hi");
    assertThrows(TypeError, () => d.decode(bytes([0x80])));
    assertThrows(TypeError, () => d.decode(bytes([0xE0, 0x41])));
    assertThrows(TypeError, () => d.decode(bytes([0xE0])));
    assertThrows(TypeError, () => d.decode(bytes([0xC0])));

    // Stream mode with valid partial: pending, no error.
    const d2 = new TextDecoder("utf-8", { fatal: true });
    assert(d2.decode(bytes([0xE2, 0x98]), { stream: true }), "");
    assert(d2.decode(bytes([0x83])), "☃");

    // Stream + flush with partial pending → error on flush.
    const d3 = new TextDecoder("utf-8", { fatal: true });
    assert(d3.decode(bytes([0xE2, 0x98]), { stream: true }), "");
    assertThrows(TypeError, () => d3.decode());
}

function test_decoder_stream() {
    // Split a 4-byte sequence at every boundary and reassemble.
    const seq = [0xF0, 0x9F, 0x98, 0x80]; // U+1F600
    for (let split = 1; split < 4; split++) {
        const d = new TextDecoder();
        let out = d.decode(bytes(seq.slice(0, split)), { stream: true });
        out += d.decode(bytes(seq.slice(split)));
        assert(out, "😀");
    }

    // E0 alone deferred; second call's first byte (0x41) is an invalid
    // continuation, so we emit U+FFFD eagerly and re-read 0x41 as ASCII.
    const d = new TextDecoder();
    assert(d.decode(bytes([0xE0]), { stream: true }), "");
    assert(d.decode(bytes([0x41])), "�A");
}

function test_decoder_brand() {
    assertThrows(TypeError, () => TextDecoder.prototype.decode.call({}));
    const enc_get =
        Object.getOwnPropertyDescriptor(TextDecoder.prototype, "encoding").get;
    assertThrows(TypeError, () => enc_get.call({}));
    const fatal_get =
        Object.getOwnPropertyDescriptor(TextDecoder.prototype, "fatal").get;
    assertThrows(TypeError, () => fatal_get.call({}));
    // Constructor without `new`.
    assertThrows(TypeError, () => TextDecoder());
}

test_encoder_basic();
test_encoder_lone_surrogates();
test_encode_into_basic();
test_encode_into_partial();
test_encode_into_argument_errors();
test_encoder_brand();
test_decoder_basic();
test_decoder_input_types();
test_decoder_label();
test_decoder_options();
test_decoder_bom();
test_decoder_invalid_sequences();
test_decoder_fatal();
test_decoder_stream();
test_decoder_brand();
