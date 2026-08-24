// Regression test for the per-runtime timezone offset cache. The cache must
// return the same offset as a fresh lookup for repeated queries, survive
// local-string round trips, handle Date limit values, and invalidate when the
// TZ environment variable changes (on platforms where the C library honors TZ).
import { assert } from "./assert.js";
import * as std from "qjs:std";
import * as os from "qjs:os";

function is_windows() {
    const os = std.getenv("OS");
    return typeof os === "string" && os.indexOf("Windows") >= 0;
}

function test_repeated_lookup() {
    const d = new Date(1234567890000);
    const tz = d.getTimezoneOffset();
    for (let i = 0; i < 1000; i++)
        assert(d.getTimezoneOffset(), tz, "repeated timezone offset");
}

function test_roundtrip() {
    const times = [
        0, 1000, 946684800000, 1234567890000, -1234567890000,
        1583650799000, 1583650800000, 1604210399000, 1604210400000,
    ];
    for (const t of times) {
        const d = new Date(t);
        assert(Date.parse(d.toString()), t, "Date local string roundtrip");
    }
}

function test_extreme() {
    for (const t of [-8640000000000000, 8640000000000000]) {
        const d = new Date(t);
        const tz = d.getTimezoneOffset();
        assert(typeof tz, "number", "Date limit timezone offset type");
        assert(isFinite(tz), true, "Date limit timezone offset finite");
    }
}

function test_tz_change() {
    // On Windows, GetTimeZoneInformation reads the registry and does not honor
    // the TZ environment variable, so skip the TZ mutation checks there.
    if (is_windows())
        return;

    const old_tz = std.getenv("TZ");
    try {
        const t = 1234567890000;
        std.setenv("TZ", "UTC0");
        let d = new Date(t);
        for (let i = 0; i < 16; i++)
            assert(d.getTimezoneOffset(), 0, "UTC timezone offset cache");

        // The cache picks up a TZ change at most JS_TZ_OFFSET_CACHE_TTL_MS (1s)
        // later, so poll instead of asserting immediately.
        std.setenv("TZ", "QST8");
        assert(wait_for_offset(d, 480, 3000), true, "TZ change invalidates cache");
        assert(Date.parse(d.toString()), t, "TZ change local string roundtrip");

        std.setenv("TZ", "UTC0");
        assert(wait_for_offset(d, 0, 3000), true, "TZ change back to UTC");
    } finally {
        if (old_tz === undefined)
            std.unsetenv("TZ");
        else
            std.setenv("TZ", old_tz);
    }
}

function wait_for_offset(d, expected, timeout_ms) {
    const deadline = Date.now() + timeout_ms;
    while (Date.now() < deadline) {
        if (d.getTimezoneOffset() === expected)
            return true;
        os.sleep(50);
    }
    return d.getTimezoneOffset() === expected;
}

test_repeated_lookup();
test_roundtrip();
test_extreme();
test_tz_change();
