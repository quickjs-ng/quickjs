---
artifact_contract: ce-unified-plan/v1
artifact_readiness: implementation-ready
execution: code
product_contract_source: ce-plan-bootstrap
type: fix
created: 2026-08-09
---

# fix: Correct int/int32_t pointer mismatches blocking build on ESP-IDF

**Target repo:** quickjs (this repo — fork of quickjs-ng/quickjs)

---

## Summary

On any target whose `stdint.h` defines `int32_t` as `long int` rather than `int` (ESP-IDF/ESP32, all chips, since ESP-IDF v5.0), `quickjs.c` fails to compile under GCC 14+ because `-Wincompatible-pointer-types` is an error by default. Six call sites across five local variables pass an `int *` where an `int32_t *` is expected, or the reverse. Both types are 32-bit signed everywhere this project targets, so this is a pure type-strictness build blocker with no behavioral effect — the fix is retyping five local variables to match the callee signature they're already used with everywhere else in the file.

## Problem Frame

- `int` and `int32_t` are distinct, incompatible C types whenever `int32_t` is a `long int` typedef, even though both are 32-bit signed (C11 6.2.5p4, 6.7.6.1p2). Passing `int *` where `int32_t *` is expected is a constraint violation requiring a diagnostic, and GCC 14+ promotes `-Wincompatible-pointer-types` from warning to error by default.
- Verified in this repo's current `quickjs.c` (HEAD): 4 local variables are declared `int` but passed by address to a function expecting `int32_t *` (across the 6 total call sites — `find_line_num`'s `v` is used at 2 call sites), and 1 local variable is declared `int32_t` but passed by address to a function expecting `int *` — the reverse mismatch, and the outlier versus every other call site of that function.
- No signature changes are needed: `JS_ToInt32`, `JS_ToInt32Free`, `get_sleb128` all take `int32_t *pres`/`pval` consistently elsewhere; `JS_ToInt32Clamp` takes `int *pres` at 18 of its 19 call sites. The fix is exclusively retyping the 5 local variables to match the signatures already in use everywhere else.
- Reproduced locally without ESP32 hardware or a GCC 14 install: shimming `int32_t` to a distinct-but-same-width type (`typedef long int xtensa_int32_t; #define int32_t xtensa_int32_t`) and compiling with `clang -std=gnu11 -Werror=incompatible-pointer-types -c quickjs.c -include xtensa_shim.h` reproduces all 6 errors at the exact lines below (Apple clang enforces the same `-Wincompatible-pointer-types` diagnostic as GCC).

## Requirements

- R1: `quickjs.c` must compile cleanly (no `-Wincompatible-pointer-types` diagnostics) when `int32_t` is a distinct 32-bit type from `int`, verified via the clang/GCC shim reproduction.
- R2: No callee signature (`JS_ToInt32`, `JS_ToInt32Free`, `JS_ToInt32Clamp`, `get_sleb128`) changes — this is a call-site-only fix.
- R3: No behavioral change — `int` and `int32_t` hold identical 32-bit signed values on every platform this project currently targets, so this is a pure type-correctness fix, not a logic change.

## Key Technical Decisions

- **KTD1: Retype the local variable at each call site to match the callee's existing parameter type, rather than changing the callee.** The callees (`JS_ToInt32`, `JS_ToInt32Free`, `get_sleb128` at `int32_t *`; `JS_ToInt32Clamp` at `int *`) are each used consistently at every other call site in the file — 18 of 19 `JS_ToInt32Clamp` calls already pass `int`. Changing a callee signature would require touching every other call site instead of just the outlier, and would be a much larger, riskier diff for zero behavioral gain.

## Implementation Units

### U1. Fix `find_line_num`'s `v` (2 call sites of `get_sleb128`)

**Goal:** `v` in `find_line_num` is declared `int32_t` instead of `int`, matching `get_sleb128(int32_t *pval, ...)`.

**Requirements:** R1, R2, R3

**Dependencies:** None

**Files:**
- `quickjs.c` (function `find_line_num`, local variable declaration `int new_line_num, new_col_num, line_num, col_num, pc, v, ret;`, and its two `get_sleb128(&v, ...)` call sites)

**Approach:** Split `v` out of the existing combined `int ...` declaration into its own `int32_t v;` declaration (or move it to a separate line), leaving the other locals (`new_line_num`, `new_col_num`, `line_num`, `col_num`, `pc`, `ret`) as `int` since they are never passed to an `int32_t *` parameter.

**Test scenarios:**
- Happy path: after the change, `clang -std=gnu11 -Werror=incompatible-pointer-types -c quickjs.c -o /dev/null -I. -include xtensa_shim.h` (shim per Problem Frame) produces no error referencing `find_line_num`'s `get_sleb128` calls.
- Regression: `find_line_num` still compiles and behaves identically under the normal (non-shimmed) build, since `int32_t` and `int` are bit-identical on every currently supported platform. Test expectation: none beyond the normal build/test suite passing — this is a type-only change with no value-space difference on supported platforms.

**Verification:** The shim compile command above no longer reports errors at the two `get_sleb128(&v, ...)` lines in `find_line_num`, and the project's normal build (`cmake`/`make` or the repo's existing build script) still succeeds and passes `run-test262`/existing test suite unaffected.

### U2. Fix `js_parseInt`'s `radix`

**Goal:** `radix` in `js_parseInt` is declared `int32_t` instead of `int`, matching `JS_ToInt32(..., int32_t *pres, ...)`.

**Requirements:** R1, R2, R3

**Dependencies:** None

**Files:**
- `quickjs.c` (function `js_parseInt`, local variable declaration `int radix, flags;`)

**Approach:** Split `radix` into its own `int32_t radix;` declaration; leave `flags` as `int` (not passed to `JS_ToInt32`).

**Test scenarios:**
- Happy path: shim compile no longer errors at `js_parseInt`'s `JS_ToInt32(ctx, &radix, argv[1])` call.
- Regression: `parseInt()` behavior in the JS test suite is unchanged (value range and semantics identical, since `int32_t` and `int` are bit-identical here).

**Verification:** Shim compile clean at this line; normal build and test suite pass unaffected.

### U3. Fix `remainingElementsCount_add`'s `remainingElementsCount`

**Goal:** `remainingElementsCount` is declared `int32_t` instead of `int`, matching `JS_ToInt32Free(..., int32_t *pres, ...)`.

**Requirements:** R1, R2, R3

**Dependencies:** None

**Files:**
- `quickjs.c` (function `remainingElementsCount_add`, local variable declaration `int remainingElementsCount;`)

**Approach:** Change the single declaration to `int32_t remainingElementsCount;`. The rest of the function (arithmetic, comparisons, `js_int32()` call) is unaffected since the value space is identical.

**Test scenarios:**
- Happy path: shim compile no longer errors at this function's `JS_ToInt32Free(ctx, &remainingElementsCount, val)` call.
- Regression: `Promise.all`/`Promise.allSettled`/`Promise.any` resolution-counting behavior (which this function backs) is unchanged in the test suite.

**Verification:** Shim compile clean at this line; normal build and test suite pass unaffected.

### U4. Fix `js_promise_all_resolve_element`'s `index`

**Goal:** `index` is declared `int32_t` instead of `int`, matching `JS_ToInt32(..., int32_t *pres, ...)`.

**Requirements:** R1, R2, R3

**Dependencies:** None

**Files:**
- `quickjs.c` (function `js_promise_all_resolve_element`, local variable declaration `int is_zero, index;`)

**Approach:** Split `index` into its own `int32_t index;` declaration; leave `is_zero` as `int` (not passed to `JS_ToInt32`).

**Test scenarios:**
- Happy path: shim compile no longer errors at this function's `JS_ToInt32(ctx, &index, func_data[1])` call.
- Regression: Promise combinator element-resolution behavior in the test suite is unchanged.

**Verification:** Shim compile clean at this line; normal build and test suite pass unaffected.

### U5. Fix `js_atomics_notify`'s `count` (the reverse-direction mismatch)

**Goal:** `count` is declared `int` instead of `int32_t`, matching `JS_ToInt32Clamp(..., int *pres, ...)` and the convention used by 18 of the 19 other `JS_ToInt32Clamp` call sites in the file.

**Requirements:** R1, R2, R3

**Dependencies:** None

**Files:**
- `quickjs.c` (function `js_atomics_notify`, local variable declaration `int32_t count, n;`)

**Approach:** Split `count` out into its own `int count;` declaration. Leave `n` as `int32_t` — it is never passed to `JS_ToInt32Clamp` and is only compared against/assigned from `count`, which remains a valid comparison between two 32-bit signed types.

**Test scenarios:**
- Happy path: shim compile no longer errors at this function's `JS_ToInt32Clamp(ctx, &count, argv[2], 0, INT32_MAX, 0)` call.
- Edge case: `count`/`n` comparisons and arithmetic elsewhere in `js_atomics_notify` (e.g. `count > 0`, assignments to `n`) still compile and behave identically, since both remain 32-bit signed.
- Regression: `Atomics.notify()` behavior in the test suite is unchanged.

**Verification:** Shim compile clean at this line; normal build and test suite pass unaffected.

## Scope Boundaries

### Deferred to Follow-Up Work

- No change to `JS_ToInt32`, `JS_ToInt32Free`, `JS_ToInt32Clamp`, or `get_sleb128`'s signatures — see KTD1.
- No compiler-pragma or warning-suppression approach — the issue and its reproduction make clear the correct fix is the underlying type mismatch, not silencing the diagnostic.
- No broader audit of every other `int`/`int32_t` pointer pass-by-address site in the codebase beyond the 6 the issue identified and this plan verified against current HEAD — a fresh, targeted shim-compile pass (as used for verification here) is the natural way to check for any other latent occurrences, but is out of scope for this fix.

## Definition of Done

- All 5 local variable retypes (U1–U5) applied, covering all 6 flagged call sites.
- The shim-based reproduction (`clang -std=gnu11 -Werror=incompatible-pointer-types -c quickjs.c -o /dev/null -I. -include xtensa_shim.h`, or an available real GCC 14 if one is present) compiles with zero `-Wincompatible-pointer-types` errors.
- The project's normal build and existing test suite (test262 runner / `make test` or equivalent) still pass, confirming no behavioral change.
- No other files modified.
