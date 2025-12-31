# Design Document: `QJS_WASI_REACTOR`

## 1. Context and Scope

QuickJS-ng currently supports WASI (WebAssembly System Interface) as a build target, producing a `.wasm` binary that can run under WASI runtimes like `wasmtime`. This build uses the standard WASI "command" model where the binary has a `_start()` entry point, runs to completion, and exits.

This design introduces a **WASI reactor** build variant that enables QuickJS to be embedded in JavaScript host environments (browsers, Node.js, Deno) with a **re-entrant execution model**. Instead of blocking in an event loop, the reactor yields control back to the JavaScript host after processing available work, allowing the host's event loop to run (handling DOM events, network I/O, etc.) before resuming QuickJS execution.

### Background

- **WASI command**: Has `_start()`, runs once, blocks in event loop, exits
- **WASI reactor**: Exports functions, no `_start()`, host controls execution flow

The existing WASI command build works for CLI-style usage under `wasmtime`/`wasmer`, but cannot be embedded in browser/Node.js environments where:
1. Blocking is not allowed (would freeze the UI/event loop)
2. The host needs to interleave its own event processing with QuickJS execution
3. Timer scheduling should integrate with the host's `setTimeout`/`queueMicrotask`

### Reference Implementation

Go's WebAssembly support (`$GOROOT/lib/wasm/wasm_exec.js`) uses a similar pattern:
- WASM module exports `run()` and `resume()` functions
- Host provides imports for time, I/O, and scheduler integration
- `runtime.scheduleTimeoutEvent` import lets Go request wake-ups from the host

## 2. Goals and Non-Goals

### Goals

- **Re-entrant execution**: QuickJS processes pending microtasks/timers and returns control to the host, rather than blocking
- **Host event loop integration**: Enable `queueMicrotask()` scheduling in the host to yield between QuickJS iterations
- **Timer integration**: Host can query when the next QuickJS timer fires and schedule a wake-up
- **Minimal changes**: Reuse existing WASI infrastructure; avoid duplicating code or adding new files where possible
- **Clean separation**: Use `#ifdef QJS_WASI_REACTOR` to clearly delineate reactor-specific code

### Non-Goals

- **Pure WASM (no WASI)**: We will continue using WASI for memory allocation, stdout/stderr, clock functions, and filesystem. A pure WASM build without WASI dependencies is not in scope.
- **Custom host imports for I/O**: Console output, file I/O, and time functions continue to use WASI. We do not add custom `qjs_host.*` imports.
- **Multiple simultaneous contexts**: The reactor exports operate on a single global runtime/context. Supporting multiple independent contexts via handles is not in scope.
- **Async I/O integration**: File and network I/O remain synchronous via WASI. Integrating with host async APIs (fetch, streams) is not in scope.
- **Worker thread support**: The `os.Worker` API is already disabled for WASI builds and remains so.

## 3. Design Overview

### System Context Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    JavaScript Host                          │
│  (Browser / Node.js / Deno / Bun)                           │
│                                                             │
│  ┌─────────────────────────────────-────────────────────┐   │
│  │              Host JavaScript                         │   │
│  │  - Loads WASM module                                 │   │
│  │  - Provides WASI imports (via polyfill or node:wasi) │   │
│  │  - Calls qjs_run(), qjs_eval(), qjs_loop_once()      │   │
│  │  - Schedules next iteration via queueMicrotask()     │   │
│  │  - Schedules timer wake-ups via setTimeout()         │   │
│  └──────────────────────┬───────────-───────────────────┘   │
│                         │                                   │
│                         ▼                                   │
│  ┌──────────────────────────────────-───────────────────┐   │
│  │              qjs.wasm (WASI Reactor)                 │   │
│  │                                                      │   │
│  │  Exports:                                            │   │
│  │  - qjs_init() → i32                                  │   │
│  │  - qjs_init_argv(argc, argv) → i32                   │   │
│  │  - qjs_eval(code, len, filename, is_module) → i32    │   │
│  │  - qjs_loop_once() → i32 (timeout_ms or status)      │   │
│  │  - qjs_destroy() → void                              │   │
│  │  - malloc(size) → ptr, free(ptr) → void              │   │
│  │                                                      │   │
│  │  Imports: wasi_snapshot_preview1.*                   │   │
│  └─────────────────────────────────-────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### APIs

#### New C API (quickjs-libc.h)

```c
/*
 * Run one iteration of the event loop (non-blocking).
 *
 * Executes all pending microtasks (promise jobs), then checks timers
 * and runs at most one expired timer callback.
 *
 * Returns:
 *   > 0: Next timer fires in this many milliseconds; call again after delay
 *     0: More work pending; call again immediately (via queueMicrotask)
 *    -1: No pending work; event loop is idle
 *    -2: An exception occurred; call js_std_dump_error() for details
 */
int js_std_loop_once(JSContext *ctx);
```

This single function replaces the need for separate "step" and "get timeout" functions by encoding both status and timeout in the return value.

#### WASM Exports (qjs.c, under `#ifdef QJS_WASI_REACTOR`)

| Export | Signature | Description |
|--------|-----------|-------------|
| `qjs_init` | `() → i32` | Initialize empty runtime (no script). Use `qjs_eval()` to run code. Returns 0 on success, -1 on error. |
| `qjs_init_argv` | `(argc: i32, argv: i32) → i32` | Initialize with CLI arguments. Same args as `qjs` CLI (e.g., `["qjs", "--std", "script.js"]`). Returns 0 on success, -1 on error. |
| `qjs_eval` | `(code: i32, len: i32, filename: i32, is_module: i32) → i32` | Evaluate JS code. `filename` used for errors and relative imports (0 for `<eval>`). Returns 0 on success, -1 on error. |
| `qjs_loop_once` | `() → i32` | Run one loop iteration. Returns timeout_ms (>0), 0 (pending), -1 (idle), or -2 (error). |
| `qjs_destroy` | `() → void` | Free runtime and context. |
| `malloc` | `(size: i32) → i32` | Allocate memory (for host to build argv array or code strings). |
| `free` | `(ptr: i32) → void` | Free memory. |

#### Usage Example (JavaScript Host)

**Simple usage with `qjs_init()` + `qjs_eval()`:**

```javascript
// Initialize empty runtime
if (exports.qjs_init() !== 0) throw new Error("init failed");

// Evaluate code
const code = 'console.log("Hello from QuickJS!")';
const codePtr = writeString(code);
exports.qjs_eval(codePtr, code.length, 0, 0);
free(codePtr);

// Drive event loop
function loop() {
    const result = exports.qjs_loop_once();
    if (result === 0) queueMicrotask(loop);
    else if (result > 0) setTimeout(loop, result);
}
loop();
```

**With CLI arguments using `qjs_init_argv()`:**

```javascript
// Build argv in WASM memory: ["qjs", "--std", "/app/script.js"]
const args = ["qjs", "--std", "/app/script.js"];
const argvPtrs = args.map(s => writeString(s));
const argvPtr = malloc(argvPtrs.length * 4);
new Uint32Array(memory.buffer, argvPtr, argvPtrs.length).set(argvPtrs);

// Initialize and load script (via WASI filesystem)
if (exports.qjs_init_argv(args.length, argvPtr) !== 0) {
    throw new Error("init failed");
}

// Drive event loop (same as above)
function loop() {
    const result = exports.qjs_loop_once();
    if (result === 0) queueMicrotask(loop);
    else if (result > 0) setTimeout(loop, result);
}
loop();
```

### Components and Interactions

#### 1. `js_std_loop_once()` Implementation

Added to `quickjs-libc.c`, approximately 35 lines:

```
js_std_loop_once(ctx)
    │
    ├─► Execute all pending jobs via JS_ExecutePendingJob()
    │   (loops until no more jobs)
    │
    ├─► Check for expired timers via js_os_run_timers()
    │   (runs at most one timer callback)
    │
    ├─► If JS_IsJobPending() or timers exist:
    │   └─► Return next timer delay (or 0 if jobs pending)
    │
    └─► Else return -1 (idle)
```

#### 2. Reactor Entry Points in `qjs.c`

At the end of `qjs.c`, guarded by `#if defined(__wasi__) && defined(QJS_WASI_REACTOR)`:

- Static globals: `reactor_rt`, `reactor_ctx`, `reactor_ready`
- `qjs_init()`: Initialize empty runtime (calls `qjs_init_argv` with no args)
- `qjs_init_argv(argc, argv)`: Parses CLI args, creates runtime/context, loads script (same logic as `main()` but stops before event loop)
- `qjs_eval(code, len, filename, is_module)`: Evaluate JS code
- `qjs_loop_once()`: Delegates to `js_std_loop_once()`
- `qjs_destroy()`: Cleanup

#### 3. Build Configuration

In `CMakeLists.txt`, new option and target:

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
    option(QJS_WASI_REACTOR "Build WASI reactor (re-entrant, no _start)" OFF)

    if(QJS_WASI_REACTOR)
        add_executable(qjs_wasi_reactor ...)
        target_compile_definitions(qjs_wasi_reactor PRIVATE QJS_WASI_REACTOR)
        target_link_options(qjs_wasi_reactor PRIVATE
            -mexec-model=reactor
            -Wl,--export=qjs_init
            -Wl,--export=qjs_init_argv
            -Wl,--export=qjs_eval
            -Wl,--export=qjs_loop_once
            -Wl,--export=qjs_destroy
            -Wl,--export=malloc
            -Wl,--export=free
        )
    endif()
endif()
```

### Data Storage

No persistent data storage is involved. Runtime state exists in WASM linear memory managed by WASI libc. The single global `JSRuntime *rt` and `JSContext *ctx` are static variables in the WASM module.

## 4. Alternatives Considered

### Alternative A: Pure WASM (No WASI)

**Description**: Build QuickJS targeting bare `wasm32` without WASI, providing all I/O via custom JavaScript imports.

**Trade-offs**:
- (+) Smaller binary, no WASI overhead
- (+) Full control over all I/O
- (-) Must implement/import: `malloc`/`free`, `clock_gettime`, `write()`, `read()`, etc.
- (-) No filesystem support without significant effort
- (-) ~300+ lines of additional harness code to provide imports
- (-) Cannot reuse existing WASI-SDK toolchain and CI

**Rejection reason**: Significantly more implementation effort for marginal benefits. WASI polyfills exist for browsers, making the WASI-based approach viable everywhere.

### Alternative B: Emscripten-based Build

**Description**: Use the existing Emscripten `qjs_wasm` target with modifications for re-entrancy.

**Trade-offs**:
- (+) Emscripten has mature browser support
- (+) Automatic memory management, async support
- (-) Different toolchain from WASI (two WASM paths to maintain)
- (-) Emscripten adds runtime overhead (~100KB+)
- (-) Different API patterns (ccall/cwrap vs direct exports)

**Rejection reason**: Maintaining two separate WASM toolchains increases complexity. WASI is the emerging standard, and polyfills bridge the browser gap.

### Alternative C: Separate `qjs-wasm.c` File

**Description**: Create a new C file for reactor entry points instead of adding `#ifdef` to `qjs.c`.

**Trade-offs**:
- (+) No changes to existing `qjs.c`
- (+) Clear separation of concerns
- (-) Code duplication (context creation, module init)
- (-) Additional file to maintain
- (-) Harder to share helper functions

**Rejection reason**: The reactor entry points share initialization logic with the CLI. Using `#ifdef` at the end of `qjs.c` keeps related code together.

### Alternative D: Two New API Functions (step + get_timeout)

**Description**: Expose `js_std_loop_step()` and `js_std_get_timeout()` as separate functions.

**Trade-offs**:
- (+) More explicit API
- (+) Timeout query doesn't require running a step
- (-) Two functions instead of one
- (-) Two WASM exports instead of one
- (-) More API surface to document/maintain

**Rejection reason**: A single `js_std_loop_once()` function that returns encoded status+timeout is simpler and sufficient for the use case. The harness always needs both pieces of information together.

## 5. Cross-Cutting Concerns

### Security

**Sandbox boundaries**: QuickJS running in WASM inherits the WASM security model—memory isolation, no direct system access. All I/O goes through WASI imports which the host controls.

**WASI capabilities**: The harness configures WASI with minimal capabilities:
- No filesystem access by default (can be enabled by host)
- stdout/stderr for console output
- Clock access for timers

**Code evaluation**: The `qjs_eval` export allows arbitrary JS execution. This is intentional—the host is responsible for controlling what code is passed to the engine. The WASM sandbox prevents escape to the host system.

### Privacy

No privacy-specific concerns. QuickJS in WASM has no network access, no persistent storage, and no access to host APIs beyond what the harness explicitly provides.

### Observability

**Console output**: `console.log` in QuickJS writes to WASI stdout (fd 1). The harness can intercept this by providing a custom `fd_write` implementation or by capturing the WASI stdout buffer.

**Errors**: Exceptions are printed to stderr via existing `js_std_dump_error()`. The `qjs_loop_once()` return value (-2) signals an error occurred.

**Debugging**: The WASM module can be built with debug info. Browser DevTools and Node.js support WASM debugging with source maps.

### Portability

**WASI polyfills**: For browser environments, WASI polyfill libraries provide the required imports. Node.js has native support via `node:wasi`.

**Feature detection**: The harness should check for `WebAssembly.instantiateStreaming` support and fall back to `instantiate` with `fetch().then(r => r.arrayBuffer())` for older environments.

## Appendix: File Changes Summary

| File | Change Type | ~Lines | Description |
|------|-------------|--------|-------------|
| `quickjs-libc.h` | Modify | +3 | Add `js_std_loop_once()` declaration |
| `quickjs-libc.c` | Modify | +40 | Add `js_std_loop_once()` implementation |
| `qjs.c` | Modify | +220 | Add `#ifdef QJS_WASI_REACTOR` section with `qjs_init`, `qjs_init_argv`, `qjs_eval`, `qjs_loop_once`, `qjs_destroy` |
| `CMakeLists.txt` | Modify | +25 | Add `QJS_WASI_REACTOR` option and `qjs_wasi_reactor` target |

**Total: ~290 lines changed/added**
