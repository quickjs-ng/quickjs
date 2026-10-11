# Security Policy

We no longer accept private security reports nor issue CVEs. Please file bug
reports in the [issue tracker](https://github.com/quickjs-ng/quickjs/issues).

## Out of Scope

**Bytecode hardening is out of scope.** QuickJS bytecode is not designed for
adversarial input. Loading untrusted bytecode (e.g. via `JS_ReadObject` with
`JS_READ_OBJ_BYTECODE`) is equivalent to executing untrusted native code, and
crashes, memory corruption, or sandbox escapes resulting from malformed or
malicious bytecode are not considered security vulnerabilities. Only load
bytecode you produced yourself or obtained from a trusted source. See the
documentation for `JS_READ_OBJ_BYTECODE` and `JS_READ_OBJ_SAB` for details.
