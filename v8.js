import * as std from "std"
import * as os from "os"

argv0 = realpath(argv0)
const tweak = realpath("v8-tweak.js")
const dir = "test262/implementation-contributed/v8/mjsunit"

const exclude = [
    "arguments-indirect.js",        // implementation-defined
    "array-concat.js",              // slow
    "array-isarray.js",             // unstable output due to stack overflow
    "ascii-regexp-subject.js",      // slow
    "asm-directive.js",             // v8 specific
    "disallow-codegen-from-strings.js", // --disallow-code-generation-from-strings
    "cyclic-array-to-string.js",    // unstable output due to stack overflow
    "error-tostring.js",            // unstable output due to stack overflow
    "regexp.js",                    // invalid, legitimate early SyntaxError
    "regexp-capture-3.js",          // slow
    "regexp-indexof.js",            // deprecated RegExp.lastMatch etc.
    "regexp-static.js",             // deprecated RegExp static properties.
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
    if (source.includes('Realm.create()')) continue // TODO support Realm object
    if (source.includes('// MODULE')) continue // TODO support modules
    if (source.includes('// Files:')) continue // TODO support includes
    // exclude tests that use V8 intrinsics like %OptimizeFunctionOnNextCall
    if (source.includes ("--allow-natives-syntax")) continue
    // exclude tests that use V8 extensions
    if (source.includes ("--expose-externalize-string")) continue
    let env = {}, envstr = ""
    for (let s of source.matchAll(/environment variables:(.+)/ig)) {
        for (let m of s[1].matchAll(/\s*([\S]+)=([\S]+)/g)) {
            env[m[1]] = m[2]
            envstr += ` ${m[1]}=${m[2]}`
        }
    }
    print(`=== ${file}${envstr}`)
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
