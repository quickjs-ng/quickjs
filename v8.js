import * as std from "std"
import * as os from "os"

argv0 = realpath(argv0)
const tweak = realpath("v8-tweak.js")
const dir = "test262/implementation-contributed/v8/mjsunit"

const exclude = [
    "array-concat.js",              // slow
    "ascii-regexp-subject.js",      // slow
    "regexp.js",                    // invalid, legitimate early SyntaxError
    "regexp-capture-3.js",          // slow
    "string-replace.js",            // unstable output

    "mjsunit-assertion-error.js",
    "mjsunit.js",
    "mjsunit_suppressions.js",

    "verify-assert-false.js",       // self check
    "verify-check-false.js",        // self check
]

let files = scriptArgs.slice(1) // run only these tests
if (files.length === 0) files = os.readdir(dir)[0].sort()

for (const file of files) {
    if (!file.endsWith(".js")) continue
    if (exclude.includes(file)) continue
    const source = std.loadFile(dir + "/" + file)
    if (/^(im|ex)port/m.test(source)) continue // TODO support modules
    if (source.includes('// Files:')) continue // TODO support includes
    // exclude tests that use V8 intrinsics like %OptimizeFunctionOnNextCall
    if (source.includes ("--allow-natives-syntax")) continue
    // exclude tests that use V8 extensions
    if (source.includes ("--expose-externalize-string")) continue
    const env =
        source.match(/environment variables:.*TZ=(?<TZ>[\S]+)/i)?.groups
    print(`=== ${file}`)
    // the fixed --stack-size is necessary to keep output of stack overflowing
    // tests stable; their stack traces are somewhat arbitrary otherwise
    const args = [argv0, "--stack-size", `${2048 * 1024}`,
                  "-I", "mjsunit.js", "-I", tweak, file]
    const opts = {block:true, cwd:dir, env:env, usePath:false}
    os.exec(args, opts)
}

function realpath(path) {
    return os.realpath(path)[0]
}
