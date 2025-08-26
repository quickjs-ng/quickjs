import { assert, assertThrows } from "./assert.js";

function test_base64() {
    function bytesToBinString(bytes) {
        let s = "";
        for (let i = 0; i < bytes.length; i++)
            s += String.fromCharCode(bytes[i] & 0xff);
        return s;
    }

    function canonicalizeBase64(s) {
        const alpha = /[A-Za-z0-9+/=]/g;
        let t = (s.match(alpha) || []).join("");
        t = t.replace(/=/g, "");
        while (t.length % 4 !== 0) t += "=";
        return t;
    }

    const A = globalThis.atob;
    const B = globalThis.btoa;

    // 1) Canonical vectors
    const vectors = [
        ["", ""],
        ["f", "Zg=="],
        ["fo", "Zm8="],
        ["foo", "Zm9v"],
        ["foob", "Zm9vYg=="],
        ["fooba", "Zm9vYmE="],
        ["foobar", "Zm9vYmFy"],
        ["\x00", "AA=="],
        ["\x00\x00", "AAA="],
        ["\x00\x00\x00", "AAAA"],
        ["\xE9", "6Q=="], // é
    ];
    for (const [plain, b64] of vectors) {
        assert(B(plain), b64, "btoa vector");
        assert(A(b64), plain, "atob vector");
    }

    // 2) Full-byte roundtrip
    const allBytes = new Uint8Array(256);
    for (let i = 0; i < 256; i++) allBytes[i] = i;
    const binAll = bytesToBinString(allBytes);
    assert(A(B(binAll)), binAll, "roundtrip 0..255");

    // 3) Padding shapes
    function expectPad(len) {
        const u = new Uint8Array(len);
        for (let i = 0; i < len; i++) u[i] = i & 255;
        const s = bytesToBinString(u);
        const b = B(s);
        const padCount = (b.match(/=/g) || []).length;
        const expected = (3 - (len % 3)) % 3;
        assert(padCount, expected, "padding count");
        assert(b.length % 4, 0, "output multiple of 4");
        assert(A(b), s, "decoded payload mismatch");
    }
    [0,1,2,3,4,5,6,7,8,9,10,255,256,257].forEach(expectPad);

    // 4) atob invalid/tolerant inputs
    function expectInvalidOrCanonSame(inStr) {
        let threw = false, got, canon;
        try { got = A(inStr); } catch { threw = true; }
        if (threw) return;
        const norm = canonicalizeBase64(inStr);
        canon = A(norm);
        assert(got, canon, "tolerant must equal canonical");
    }
    [
        "A", "AAA", "====", "A===", "Zg=", "Zg===",
        "Zg====", // extra invalid padding
        "Zm=8", "Zm9=v", "*m9v", "mØ9v"
    ].forEach(expectInvalidOrCanonSame);

    // 5) Whitespace tolerance (spec requires ignoring)
    assert(A(" Z g = = \n\t"), "f", "atob must ignore whitespace");
    assert(A("Zg\f=="), "f", "atob must ignore form feed");

    // 6) Pure padding / weird empties
    assert(A(""), "", "empty input");
    assertThrows(DOMException, () => A("===="), "pure padding must throw");

    // 7) btoa invalid input (non-Latin1 must throw TypeError)
    const badBtoa = ["💩", "𝌆", "\uD83D", "\uDC36", "\u0100"];
    for (const s of badBtoa) {
        assertThrows(DOMException, () => B(s));
    }

    // 8) Sanity
    assert(B("test"), "dGVzdA==", "btoa test");
    assert(A("dGVzdA=="), "test", "atob test");
}

test_base64();
