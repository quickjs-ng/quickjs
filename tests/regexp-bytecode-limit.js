import {assert} from "./assert.js"

// CVE-2025-62495: compiling a regexp must reject patterns whose bytecode would
// be huge instead of exhausting memory. The register-based engine grows the
// bytecode linearly with the pattern (each 'a' emits a ~3 byte char opcode),
// so a long enough literal exceeds the 64MB lre_check_size cap and is rejected.
let caught = false
try {
    new RegExp("a".repeat(25_000_000))
} catch (e) {
    assert(e instanceof SyntaxError)
    assert(e.message, "out of memory")
    caught = true
}
assert(caught)

// A pattern comfortably under the cap still compiles.
new RegExp("a".repeat(1_000_000))
