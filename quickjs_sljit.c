/*
 * QuickJS sljit JIT compiler
 *
 * Copyright (c) 2025 QuickJS-ng contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include "quickjs.h"
#include "cutils.h"
/*
 * CONFIG_JIT must be defined before including quickjs-jit.h so that
 * the JitAux / JitFunc definitions are visible.
 */
#ifndef CONFIG_JIT
#define CONFIG_JIT
#endif

#include "quickjs-jit.h"

/* ---- sljit all-in-one inclusion ---- */
#define SLJIT_CONFIG_AUTO 1
#define SLJIT_CONFIG_STATIC 1
#include "sljit/sljit_src/sljitLir.c"

/* ---- Opcode definitions ---- */

/* Build the opcode enum (non-temp opcodes only, matching quickjs.c) */
typedef enum JitOPCodeEnum {
#define FMT(f)
#define DEF(id, size, n_pop, n_push, f) OP_ ## id,
#define def(id, size, n_pop, n_push, f)
#include "quickjs-opcode.h"
#undef def
#undef DEF
#undef FMT
    OP_COUNT,
    OP_TEMP_START = OP_nop + 1,
    OP___dummy = OP_TEMP_START - 1,
#define FMT(f)
#define DEF(id, size, n_pop, n_push, f)
#define def(id, size, n_pop, n_push, f) OP_ ## id,
#include "quickjs-opcode.h"
#undef def
#undef DEF
#undef FMT
    OP_TEMP_END,
} JitOPCodeEnum;

/* Opcode sizes (bytes) for bytecode scanning */
static const uint8_t jit_opcode_size[OP_COUNT] = {
#define FMT(f)
#define DEF(id, size, n_pop, n_push, f) size,
#define def(id, size, n_pop, n_push, f)
#include "quickjs-opcode.h"
#undef def
#undef DEF
#undef FMT
};

/* ---- Constants ---- */

#define JSV_SIZE ((sljit_sw)sizeof(JSValue))

/* For struct JSValue (non-NaN-boxing), field offsets within JSValue */
#if !(defined(JS_NAN_BOXING) && JS_NAN_BOXING) && !defined(JS_CHECK_JSVALUE)
#define JSV_U_OFF   0   /* offsetof(JSValue, u) -- always 0 */
#define JSV_TAG_OFF ((sljit_sw)offsetof(JSValue, tag))
#endif

/* ---- Register assignments ---- */

#define REG_CTX   SLJIT_S0  /* JSContext *ctx (first arg, preserved) */
#define REG_AUX   SLJIT_S1  /* JitAux *aux (second arg, preserved) */
#define REG_SP    SLJIT_S2  /* JSValue *sp (stack pointer, updated) */
#define REG_VBUF  SLJIT_S3  /* JSValue *var_buf (preserved) */
#define REG_ABUF  SLJIT_S4  /* JSValue *arg_buf (preserved) */

/* ---- Deferred jump for forward branches ---- */

typedef struct JitJumpPatch {
    struct sljit_jump *jump;
    int target_pc;
} JitJumpPatch;

/* ---- C helper functions called by JIT code via icall ---- */

/*
 * Note: In quickjs-ng, JS_DupValue/JS_FreeValue are extern functions
 * (not inlined), so we use them directly for refcount management.
 * The ctx parameter is needed for JS_DupValue/JS_FreeValue.
 */

/* Load variable/argument at index and push to stack (with refcount inc) */
static void jit_helper_get_var(JSContext *ctx, JSValue *sp,
                               JSValue *buf, sljit_sw idx)
{
    *sp = JS_DupValue(ctx, buf[idx]);
}

/* Pop from stack and store to variable/argument (frees old value) */
static void jit_helper_put_var(JSContext *ctx, JSValue *sp,
                               JSValue *buf, sljit_sw idx)
{
    JS_FreeValue(ctx, buf[idx]);
    buf[idx] = sp[-1];
}

/* Like put_var but keep value on stack (dup + store) */
static void jit_helper_set_var(JSContext *ctx, JSValue *sp,
                               JSValue *buf, sljit_sw idx)
{
    JSValue v = sp[-1];
    JS_FreeValue(ctx, buf[idx]);
    buf[idx] = JS_DupValue(ctx, v);
}

/* Free the value at sp[-1] (caller must decrement sp) */
static void jit_helper_drop(JSContext *ctx, JSValue *sp)
{
    JS_FreeValue(ctx, sp[-1]);
}

/* Duplicate sp[-1] to sp[0] with refcount (caller must increment sp) */
static void jit_helper_dup(JSContext *ctx, JSValue *sp)
{
    sp[0] = JS_DupValue(ctx, sp[-1]);
}

/* Swap sp[-1] and sp[-2] */
#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)
static void jit_helper_swap(JSValue *sp)
{
    JSValue tmp = sp[-1];
    sp[-1] = sp[-2];
    sp[-2] = tmp;
}
#endif

/* nip: a b -> b. Free a, keep b. */
static void jit_helper_nip(JSContext *ctx, JSValue *sp)
{
    JS_FreeValue(ctx, sp[-2]);
    sp[-2] = sp[-1];
}

/* Convert sp[-1] to boolean, free it. Returns 0 or 1. */
static sljit_sw jit_helper_to_bool_free(JSContext *ctx, JSValue *sp)
{
    JSValue v = sp[-1];
    int tag = JS_VALUE_GET_TAG(v);
    /* Fast path for INT and BOOL (tags 0 and 1) */
    if ((unsigned)tag <= JS_TAG_UNDEFINED) {
        return JS_VALUE_GET_INT(v);
    }
    /* Slow path: convert and free */
    {
        int res = JS_ToBool(ctx, v);
        JS_FreeValue(ctx, v);
        return res;
    }
}

/* Store return value from sp[-1] into aux, set aux->sp. */
#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)
static void jit_helper_return(JitAux *aux, JSValue *sp)
{
    aux->ret_val = sp[-1];
    aux->sp = sp - 1;
}
#endif

/*
 * Generator suspend helper: save sp and ret_val into aux.
 * The caller (JIT code) has already set sf->cur_pc via the bytecode
 * pointer baked into the generated code.  cur_sp is set by the C caller
 * after JIT returns.
 */
static void jit_helper_generator_suspend(JitAux *aux, JSValue *sp,
                                          sljit_sw suspend_code,
                                          const uint8_t *resume_pc)
{
    aux->sp = sp;
    /* 0=FUNC_RET_AWAIT, 1=FUNC_RET_YIELD, 2=FUNC_RET_YIELD_STAR → js_int32(code)
     * 3=initial_yield/return_async → JS_UNDEFINED */
    if (suspend_code <= 2)
        aux->ret_val = JS_MKVAL(JS_TAG_INT, (int32_t)suspend_code);
    else
        aux->ret_val = JS_UNDEFINED;
    aux->resume_native_addr = NULL;
    aux->resume_bc_pc = resume_pc;
}

/* Store JS_UNDEFINED as return value into aux, set aux->sp. */
#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)
static void jit_helper_return_undef(JitAux *aux, JSValue *sp)
{
    aux->ret_val = JS_UNDEFINED;
    aux->sp = sp;
}
#endif

/* ---- JIT opcode helpers ---- */

/* ---- stack manipulation ---- */

static int jit_op_nip1(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    JS_FreeValue(ctx, sp[-3]);
    sp[-3] = sp[-2];
    sp[-2] = sp[-1];
    sp--;
    aux->sp = sp;
    return 0;
}

static int jit_op_dup1(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    sp[0] = sp[-1];
    sp[-1] = JS_DupValue(ctx, sp[-2]);
    sp++;
    aux->sp = sp;
    return 0;
}

static int jit_op_dup2(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    sp[0] = JS_DupValue(ctx, sp[-2]);
    sp[1] = JS_DupValue(ctx, sp[-1]);
    sp += 2;
    aux->sp = sp;
    return 0;
}

static int jit_op_dup3(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    sp[0] = JS_DupValue(ctx, sp[-3]);
    sp[1] = JS_DupValue(ctx, sp[-2]);
    sp[2] = JS_DupValue(ctx, sp[-1]);
    sp += 3;
    aux->sp = sp;
    return 0;
}

#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)
static int jit_op_insert2(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    sp[0] = sp[-1];
    sp[-1] = sp[-2];
    sp[-2] = JS_DupValue(ctx, sp[0]);
    sp++;
    aux->sp = sp;
    return 0;
}

static int jit_op_insert3(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    sp[0] = sp[-1];
    sp[-1] = sp[-2];
    sp[-2] = sp[-3];
    sp[-3] = JS_DupValue(ctx, sp[0]);
    sp++;
    aux->sp = sp;
    return 0;
}
#endif

static int jit_op_insert4(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    sp[0] = sp[-1];
    sp[-1] = sp[-2];
    sp[-2] = sp[-3];
    sp[-3] = sp[-4];
    sp[-4] = JS_DupValue(ctx, sp[0]);
    sp++;
    aux->sp = sp;
    return 0;
}

static int jit_op_perm3(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    JSValue tmp;
    (void)ctx;
    tmp = sp[-2];
    sp[-2] = sp[-3];
    sp[-3] = tmp;
    aux->sp = sp;
    return 0;
}

static int jit_op_perm4(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    JSValue tmp;
    (void)ctx;
    tmp = sp[-2];
    sp[-2] = sp[-3];
    sp[-3] = sp[-4];
    sp[-4] = tmp;
    aux->sp = sp;
    return 0;
}

static int jit_op_perm5(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    JSValue tmp;
    (void)ctx;
    tmp = sp[-2];
    sp[-2] = sp[-3];
    sp[-3] = sp[-4];
    sp[-4] = sp[-5];
    sp[-5] = tmp;
    aux->sp = sp;
    return 0;
}

static int jit_op_rot3l(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    JSValue tmp;
    (void)ctx;
    tmp = sp[-3];
    sp[-3] = sp[-2];
    sp[-2] = sp[-1];
    sp[-1] = tmp;
    aux->sp = sp;
    return 0;
}

static int jit_op_rot3r(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    JSValue tmp;
    (void)ctx;
    tmp = sp[-1];
    sp[-1] = sp[-2];
    sp[-2] = sp[-3];
    sp[-3] = tmp;
    aux->sp = sp;
    return 0;
}

static int jit_op_rot4l(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    JSValue tmp;
    (void)ctx;
    tmp = sp[-4];
    sp[-4] = sp[-3];
    sp[-3] = sp[-2];
    sp[-2] = sp[-1];
    sp[-1] = tmp;
    aux->sp = sp;
    return 0;
}

static int jit_op_rot5l(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    JSValue tmp;
    (void)ctx;
    tmp = sp[-5];
    sp[-5] = sp[-4];
    sp[-4] = sp[-3];
    sp[-3] = sp[-2];
    sp[-2] = sp[-1];
    sp[-1] = tmp;
    aux->sp = sp;
    return 0;
}

static int jit_op_swap2(JSContext *ctx, JitAux *aux)
{
    JSValue *sp = aux->sp;
    JSValue tmp1, tmp2;
    (void)ctx;
    tmp1 = sp[-4];
    tmp2 = sp[-3];
    sp[-4] = sp[-2];
    sp[-3] = sp[-1];
    sp[-2] = tmp1;
    sp[-1] = tmp2;
    aux->sp = sp;
    return 0;
}

/* ---- Inline JSValue emit helpers ---- */

/* Push a known-constant JSValue onto the JIT stack (compile time). */
static void emit_push_const_jsv(struct sljit_compiler *C, JSValue v)
{
#if defined(JS_NAN_BOXING) && JS_NAN_BOXING
    /* JSValue = uint64_t, fits in one word on 64-bit, two on 32-bit */
    #if (SLJIT_WORD_SIZE == 64)
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), 0,
                   SLJIT_IMM, (sljit_sw)v);
    #else
    /* 32-bit NaN boxing: store low 32 bits then high 32 bits */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), 0,
                   SLJIT_IMM, (sljit_sw)(uint32_t)v);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), 4,
                   SLJIT_IMM, (sljit_sw)(uint32_t)(v >> 32));
    #endif
#elif defined(JS_CHECK_JSVALUE)
    /* Pointer mode: JSValue is a pointer, fits in a word */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), 0,
                   SLJIT_IMM, (sljit_sw)v);
#else
    /* Struct mode: JSValue = { JSValueUnion u; int64_t tag; } */
    /* Store union (word-sized, zero-extended int32) */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), JSV_U_OFF,
                   SLJIT_IMM, (sljit_sw)JS_VALUE_GET_INT(v));
    /* Store tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), JSV_TAG_OFF,
                   SLJIT_IMM, (sljit_sw)JS_VALUE_GET_TAG(v));
#endif
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
}

#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)

/* Emit icall to jit_helper_get_var(ctx, sp, buf_reg, idx), then sp++ */
static void emit_get_var(struct sljit_compiler *C,
                         sljit_s32 buf_reg, sljit_sw idx)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, buf_reg, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, idx);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS4V(P, P, P, W),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_get_var));
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
}

/* Emit icall to jit_helper_put_var(ctx, sp, buf_reg, idx), then sp-- */
static void emit_put_var(struct sljit_compiler *C,
                         sljit_s32 buf_reg, sljit_sw idx)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, buf_reg, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, idx);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS4V(P, P, P, W),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_put_var));
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
}

#endif /* NaN-boxing / CHECK mode */

/* Emit icall to jit_helper_set_var(ctx, sp, buf_reg, idx). No sp change. */
static void emit_set_var(struct sljit_compiler *C,
                         sljit_s32 buf_reg, sljit_sw idx)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, buf_reg, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, idx);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS4V(P, P, P, W),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_set_var));
}

/* Emit drop: call jit_helper_drop(ctx, sp), then sp-- */
#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)
static void emit_drop(struct sljit_compiler *C)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_drop));
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
}
#endif

/* Emit dup: call jit_helper_dup(ctx, sp), then sp++ */
#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)
static void emit_dup(struct sljit_compiler *C)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_dup));
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
}

/* Emit swap: call jit_helper_swap(sp) */
static void emit_swap(struct sljit_compiler *C)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS1V(P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_swap));
}

/* Emit nip: call jit_helper_nip(ctx, sp), then sp-- */
static void emit_nip(struct sljit_compiler *C)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_nip));
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
}
#endif

/*
 * ---- Inline fast-path emitters (struct mode only) ----
 *
 * These functions emit inline machine code for the common case (integer or
 * non-refcounted values) and fall back to calling the C helper for the
 * uncommon case.  They are guarded by the struct-mode preprocessor check
 * because NaN-boxing and CHECK modes have different JSValue layouts.
 *
 * Pattern:
 *   1. Load tag from memory
 *   2. Check tag for fast-path condition
 *   3. Fast path: do operation inline (memcpy int32+tag, arithmetic, etc.)
 *   4. Jump over slow path
 *   5. Slow path: call existing C helper
 *   6. Done label
 */

#if !(defined(JS_NAN_BOXING) && JS_NAN_BOXING) && !defined(JS_CHECK_JSVALUE)

/*
 * emit_get_var_fast: Inline get_loc/get_arg for non-refcounted values.
 *
 * For non-refcounted types (tag >= 0: INT, BOOL, NULL, UNDEFINED, FLOAT64),
 * we can simply memcpy the 16-byte JSValue to the stack without calling
 * JS_DupValue.  For refcounted types (tag < 0), fall back to helper.
 *
 * Generated code:
 *   R0 = buf[idx].tag
 *   if (R0 < 0) goto slow_path
 *   sp[0].u = buf[idx].u     (word-sized copy)
 *   sp[0].tag = R0
 *   sp += JSV_SIZE
 *   goto done
 * slow_path:
 *   call jit_helper_get_var(ctx, sp, buf, idx)
 *   sp += JSV_SIZE
 * done:
 */
static void emit_get_var_fast(struct sljit_compiler *C,
                              sljit_s32 buf_reg, sljit_sw idx)
{
    struct sljit_jump *slow_path, *done;
    sljit_sw byte_off = idx * JSV_SIZE;

    /* Load tag from buf[idx].tag into R0 */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(buf_reg), byte_off + JSV_TAG_OFF);

    /* If tag < 0 (refcounted), go slow path */
    slow_path = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                               SLJIT_R0, 0, SLJIT_IMM, 0);

    /* Fast path: copy u (word at offset 0) and tag to sp[0] */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(buf_reg), byte_off + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), JSV_U_OFF,
                   SLJIT_R1, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), JSV_TAG_OFF,
                   SLJIT_R0, 0);
    /* sp++ */
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* Slow path: call helper */
    sljit_set_label(slow_path, sljit_emit_label(C));
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, buf_reg, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, idx);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS4V(P, P, P, W),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_get_var));
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);

    /* Done */
    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * emit_put_var_fast: Inline put_loc/put_arg when old value is non-refcounted.
 *
 * If the old value's tag >= 0, we can just overwrite without calling
 * JS_FreeValue.  Otherwise, fall back to helper which frees the old value.
 *
 * Generated code:
 *   R0 = buf[idx].tag
 *   if (R0 < 0) goto slow_path
 *   buf[idx].u = sp[-1].u
 *   buf[idx].tag = sp[-1].tag
 *   sp -= JSV_SIZE
 *   goto done
 * slow_path:
 *   call jit_helper_put_var(ctx, sp, buf, idx)
 *   sp -= JSV_SIZE
 * done:
 */
static void emit_put_var_fast(struct sljit_compiler *C,
                              sljit_s32 buf_reg, sljit_sw idx)
{
    struct sljit_jump *slow_path, *done;
    sljit_sw byte_off = idx * JSV_SIZE;

    /* Load old tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(buf_reg), byte_off + JSV_TAG_OFF);

    /* If old tag < 0 (refcounted), need to free → slow path */
    slow_path = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                               SLJIT_R0, 0, SLJIT_IMM, 0);

    /* Fast path: overwrite without free */
    /* Load sp[-1].u and sp[-1].tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    /* Store to buf[idx] */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(buf_reg), byte_off + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(buf_reg), byte_off + JSV_TAG_OFF,
                   SLJIT_R1, 0);
    /* sp-- */
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* Slow path */
    sljit_set_label(slow_path, sljit_emit_label(C));
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, buf_reg, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, idx);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS4V(P, P, P, W),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_put_var));
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);

    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * emit_drop_fast: Inline drop for non-refcounted values.
 *
 * If sp[-1].tag >= 0, just decrement sp (no free needed).
 * Otherwise call jit_helper_drop + decrement.
 */
static void emit_drop_fast(struct sljit_compiler *C)
{
    struct sljit_jump *slow_path, *done;

    /* Load sp[-1].tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);

    /* If tag < 0 → slow path (need JS_FreeValue) */
    slow_path = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                               SLJIT_R0, 0, SLJIT_IMM, 0);

    /* Fast path: just sp-- */
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* Slow path */
    sljit_set_label(slow_path, sljit_emit_label(C));
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_drop));
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);

    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * emit_branch_fast: Inline if_false/if_true for JS_TAG_INT values.
 *
 * Both paths converge with the truthiness value in SLJIT_R0,
 * then a single comparison emits the branch jump.
 *
 * sense=0: branch if false (value == 0)
 * sense=1: branch if true  (value != 0)
 */
static struct sljit_jump *emit_branch_fast(struct sljit_compiler *C, int sense)
{
    struct sljit_jump *not_int, *skip_slow;

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);

    not_int = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                             SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Fast path: R0 = int32 value (truthiness), sp-- */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    skip_slow = sljit_emit_jump(C, SLJIT_JUMP);

    /* Slow path: R0 = to_bool_free result (truthiness), sp-- */
    sljit_set_label(not_int, sljit_emit_label(C));
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_to_bool_free));
    /* icall returns in SLJIT_RETURN_REG (== SLJIT_R0) */
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);

    /* Converge: R0 has truthiness value from either path */
    sljit_set_label(skip_slow, sljit_emit_label(C));
    return sljit_emit_cmp(C,
                          sense ? SLJIT_NOT_EQUAL : SLJIT_EQUAL,
                          SLJIT_R0, 0, SLJIT_IMM, 0);
}

/*
 * emit_add_sub_fast: Inline integer add/sub with overflow detection.
 *
 * Fast path: if both sp[-1] and sp[-2] have tag == JS_TAG_INT,
 * perform the 32-bit operation inline with overflow check.
 * On overflow or non-integer operands, fall back to the C helper.
 *
 * On 64-bit: sign-extend both int32 to 64-bit, add/sub in 64-bit,
 *   then check if result fits in int32 range.
 * On 32-bit: use SLJIT_ADD/SLJIT_SUB with SLJIT_SET_OVERFLOW.
 *
 * is_sub: 0 = add, 1 = sub
 */
static void emit_add_sub_fast(struct sljit_compiler *C,
                               int is_sub,
                               struct sljit_jump **exc_jumps,
                               int *n_exc_jumps)
{
    struct sljit_jump *not_int1, *not_int2, *done;
    void *helper_fn = is_sub ? (void *)qjs_jit_sub : (void *)qjs_jit_add;

    /* Load sp[-1].tag into R0 */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);

    /* Check sp[-1].tag == JS_TAG_INT */
    not_int1 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Load sp[-2].tag into R0 */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);

    /* Check sp[-2].tag == JS_TAG_INT */
    not_int2 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Both are integers. Load int32 values. */
    /* R0 = sp[-2].u.int32 (left operand) */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF);
    /* R1 = sp[-1].u.int32 (right operand) */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);

#if (SLJIT_WORD_SIZE == 64)
    /* 64-bit platform: add/sub in 64-bit, then check int32 range.
     * Since both values are sign-extended 32-bit values in 64-bit registers,
     * the result of add/sub will be at most 33 bits.  We check if the
     * result fits in int32 by sign-extending from 32 to 64 and comparing. */
    if (is_sub) {
        sljit_emit_op2(C, SLJIT_SUB, SLJIT_R0, 0,
                       SLJIT_R0, 0, SLJIT_R1, 0);
    } else {
        sljit_emit_op2(C, SLJIT_ADD, SLJIT_R0, 0,
                       SLJIT_R0, 0, SLJIT_R1, 0);
    }
    /* R1 = sign-extend R0 from 32 to 64 */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_R0, 0);
    /* If R0 != R1, overflow occurred → slow path */
    {
        struct sljit_jump *overflow = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                     SLJIT_R0, 0, SLJIT_R1, 0);
        /* No overflow: store result to sp[-2].u.int32, tag stays JS_TAG_INT */
        sljit_emit_op1(C, SLJIT_MOV_S32,
                       SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                       SLJIT_R0, 0);
        /* sp-- (pop right operand) */
        sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                       SLJIT_IMM, JSV_SIZE);
        done = sljit_emit_jump(C, SLJIT_JUMP);

        /* Overflow → slow path */
        sljit_set_label(overflow, sljit_emit_label(C));
    }
#else
    /* 32-bit platform: use SLJIT_SET_OVERFLOW */
    if (is_sub) {
        sljit_emit_op2(C, SLJIT_SUB | SLJIT_SET_OVERFLOW,
                       SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
    } else {
        sljit_emit_op2(C, SLJIT_ADD | SLJIT_SET_OVERFLOW,
                       SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
    }
    {
        struct sljit_jump *overflow = sljit_emit_jump(C, SLJIT_OVERFLOW);

        /* No overflow: store result */
        sljit_emit_op1(C, SLJIT_MOV,
                       SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                       SLJIT_R0, 0);
        /* sp-- */
        sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                       SLJIT_IMM, JSV_SIZE);
        done = sljit_emit_jump(C, SLJIT_JUMP);

        sljit_set_label(overflow, sljit_emit_label(C));
    }
#endif

    /* Slow path: fall back to C helper */
    {
        struct sljit_label *slow_label = sljit_emit_label(C);
        sljit_set_label(not_int1, slow_label);
        sljit_set_label(not_int2, slow_label);
    }
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(helper_fn));
    {
        struct sljit_jump *exc_jump = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);
        exc_jumps[(*n_exc_jumps)++] = exc_jump;
    }
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);

    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * emit_mul_fast: Inline integer multiplication.
 *
 * On 64-bit: multiply two sign-extended int32 values as 64-bit,
 *   result always fits in 63 bits. Check if result fits int32.
 *   Special case: 0 result needs sign check for -0.
 *
 * On 32-bit: just use the slow path (overflow detection for mul
 *   is complex with SLJIT on 32-bit, and SLJIT_MUL has no SET_OVERFLOW
 *   on some backends).
 */
static void emit_mul_fast(struct sljit_compiler *C,
                          struct sljit_jump **exc_jumps,
                          int *n_exc_jumps)
{
#if (SLJIT_WORD_SIZE == 64)
    struct sljit_jump *not_int1, *not_int2, *done;

    /* Check sp[-1].tag == JS_TAG_INT */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    not_int1 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Check sp[-2].tag == JS_TAG_INT */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);
    not_int2 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Load int32 values (sign-extended to 64-bit) */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);

    /* 64-bit multiply: int32 * int32 → fits in int63, no overflow possible.
     * But we need to check if result fits in int32 range. */
    sljit_emit_op2(C, SLJIT_MUL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);

    /* Check result fits int32: sign-extend from 32 and compare */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0, SLJIT_R0, 0);
    {
        struct sljit_jump *overflow = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                     SLJIT_R0, 0, SLJIT_R1, 0);

        /* Result fits int32. But check for -0: if result is 0 and either
         * operand was negative, result should be -0.0 (float). We handle
         * this by falling to slow path when result is 0 (rare case). */
        {
            struct sljit_jump *zero_check = sljit_emit_cmp(C, SLJIT_EQUAL,
                                                           SLJIT_R0, 0, SLJIT_IMM, 0);

            /* Non-zero result that fits int32: store and done */
            sljit_emit_op1(C, SLJIT_MOV_S32,
                           SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                           SLJIT_R0, 0);
            sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                           SLJIT_IMM, JSV_SIZE);
            done = sljit_emit_jump(C, SLJIT_JUMP);

            /* Zero result → slow path (might need -0.0) */
            sljit_set_label(zero_check, sljit_emit_label(C));
        }
        sljit_set_label(overflow, sljit_emit_label(C));
    }

    /* Slow path */
    {
        struct sljit_label *slow_label = sljit_emit_label(C);
        sljit_set_label(not_int1, slow_label);
        sljit_set_label(not_int2, slow_label);
    }
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(qjs_jit_mul));
    {
        struct sljit_jump *exc_jump = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);
        exc_jumps[(*n_exc_jumps)++] = exc_jump;
    }
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);

    sljit_set_label(done, sljit_emit_label(C));

#else
    /* 32-bit: call C helper directly (no inline fast path for mul) */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(qjs_jit_mul));
    {
        struct sljit_jump *exc_jump = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);
        exc_jumps[(*n_exc_jumps)++] = exc_jump;
    }
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
#endif
}

/*
 * emit_swap_fast: Inline swap of sp[-1] and sp[-2].
 *
 * Pure memory swap — no tag checks or refcounting needed.
 * Uses R0-R3 as scratch to swap both 16-byte JSValues.
 */
static void emit_swap_fast(struct sljit_compiler *C)
{
    /* R0 = sp[-1].u, R1 = sp[-1].tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    /* R2 = sp[-2].u, R3 = sp[-2].tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);
    /* sp[-1] = old sp[-2] */
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF,
                   SLJIT_R2, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_R3, 0);
    /* sp[-2] = old sp[-1] */
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_R1, 0);
}

/*
 * emit_dup_fast: Inline dup for non-refcounted values.
 *
 * If sp[-1].tag >= 0 (non-refcounted), just copy the 16-byte JSValue.
 * Otherwise fall back to jit_helper_dup for JS_DupValue.
 */
static void emit_dup_fast(struct sljit_compiler *C)
{
    struct sljit_jump *slow_path, *done;

    /* R0 = sp[-1].tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);

    /* If tag < 0 (refcounted) → slow path */
    slow_path = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                               SLJIT_R0, 0, SLJIT_IMM, 0);

    /* Fast path: copy sp[-1] to sp[0] */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), JSV_U_OFF,
                   SLJIT_R1, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), JSV_TAG_OFF,
                   SLJIT_R0, 0);
    /* sp++ */
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* Slow path: call jit_helper_dup(ctx, sp) */
    sljit_set_label(slow_path, sljit_emit_label(C));
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_dup));
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);

    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * emit_nip_fast: Inline nip (a b → b) for non-refcounted 'a'.
 *
 * If sp[-2].tag >= 0, no JS_FreeValue needed — just overwrite and sp--.
 * Otherwise fall back to jit_helper_nip.
 */
static void emit_nip_fast(struct sljit_compiler *C)
{
    struct sljit_jump *slow_path, *done;

    /* R0 = sp[-2].tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);

    /* If tag < 0 (refcounted) → slow path (need JS_FreeValue) */
    slow_path = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                               SLJIT_R0, 0, SLJIT_IMM, 0);

    /* Fast path: copy sp[-1] to sp[-2], then sp-- */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_R1, 0);
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* Slow path: call jit_helper_nip(ctx, sp) */
    sljit_set_label(slow_path, sljit_emit_label(C));
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_nip));
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);

    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * emit_relational_fast: Inline integer comparison for <, >, <=, >=.
 *
 * Fast path: both operands are JS_TAG_INT → compare via sljit_emit_op2u
 * with SLJIT_SUB | SLJIT_SET_SIG_*, then sljit_emit_op_flags to produce
 * a 0/1 boolean. No branches needed for the comparison itself.
 * Slow path: call jit_op_relational(ctx, aux, opcode).
 *
 * SLJIT_SET uses paired flags:
 *   SLJIT_SET_SIG_LESS   covers SLJIT_SIG_LESS and SLJIT_SIG_GREATER_EQUAL
 *   SLJIT_SET_SIG_GREATER covers SLJIT_SIG_GREATER and SLJIT_SIG_LESS_EQUAL
 */
static void emit_relational_fast(struct sljit_compiler *C,
                                  int opcode,
                                  struct sljit_jump **exc_jumps,
                                  int *n_exc_jumps)
{
    struct sljit_jump *not_int1, *not_int2, *done;
    sljit_s32 set_flag;  /* flag to pass to SLJIT_SUB | set_flag */
    sljit_s32 cond;      /* condition to read with sljit_emit_op_flags */

    /* Map opcode to SLJIT_SET_* and the condition to read.
     * SLJIT_SET uses paired flags, so the SET macro covers two conditions:
     *   SLJIT_SET_SIG_LESS = SLJIT_SET(SLJIT_SIG_LESS) → covers SIG_LESS & SIG_GREATER_EQUAL
     *   SLJIT_SET_SIG_GREATER = SLJIT_SET(SLJIT_SIG_GREATER) → covers SIG_GREATER & SIG_LESS_EQUAL
     */
    switch (opcode) {
    case OP_lt:
        set_flag = SLJIT_SET_SIG_LESS;
        cond = SLJIT_SIG_LESS;
        break;
    case OP_lte:
        set_flag = SLJIT_SET_SIG_GREATER;  /* covers SIG_LESS_EQUAL */
        cond = SLJIT_SIG_LESS_EQUAL;
        break;
    case OP_gt:
        set_flag = SLJIT_SET_SIG_GREATER;
        cond = SLJIT_SIG_GREATER;
        break;
    case OP_gte:
        set_flag = SLJIT_SET_SIG_LESS;    /* covers SIG_GREATER_EQUAL */
        cond = SLJIT_SIG_GREATER_EQUAL;
        break;
    default:
        set_flag = SLJIT_SET_SIG_LESS;
        cond = SLJIT_SIG_LESS;
        break;
    }

    /* Check sp[-1].tag == JS_TAG_INT */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    not_int1 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Check sp[-2].tag == JS_TAG_INT */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);
    not_int2 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Both are integers: load int32 values (sign-extended to word size) */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);

    /* Branchless comparison: SUB sets flags, op_flags reads them.
     * sljit_emit_op2u discards the SUB result, only sets flags.
     * sljit_emit_op_flags writes 0 or 1 based on the condition. */
    sljit_emit_op2u(C, SLJIT_SUB | set_flag, SLJIT_R0, 0, SLJIT_R1, 0);
    sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_R0, 0, cond);

    /* Store result as JS_BOOL to sp[-2] */
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_IMM, JS_TAG_BOOL);
    /* sp-- */
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* Slow path: call jit_op_relational(ctx, aux, opcode) */
    {
        struct sljit_label *slow_label = sljit_emit_label(C);
        sljit_set_label(not_int1, slow_label);
        sljit_set_label(not_int2, slow_label);
    }
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                   REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, opcode);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS3(W, P, P, W),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_op_relational));
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    {
        struct sljit_jump *exc_jump = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);
        exc_jumps[(*n_exc_jumps)++] = exc_jump;
    }

    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * emit_binary_logic_fast: Inline integer AND/OR/XOR.
 *
 * Fast path: both operands are JS_TAG_INT → perform bitwise op inline.
 * Slow path: call jit_op_binary_logic(ctx, aux, opcode).
 */
static void emit_binary_logic_fast(struct sljit_compiler *C,
                                    int opcode,
                                    struct sljit_jump **exc_jumps,
                                    int *n_exc_jumps)
{
    struct sljit_jump *not_int1, *not_int2, *done;
    sljit_s32 sljit_op;

    /* Map opcode to SLJIT operation */
    switch (opcode) {
    case OP_and: sljit_op = SLJIT_AND; break;
    case OP_or:  sljit_op = SLJIT_OR; break;
    case OP_xor: sljit_op = SLJIT_XOR; break;
    default:     sljit_op = SLJIT_AND; break;
    }

    /* Check sp[-1].tag == JS_TAG_INT */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    not_int1 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Check sp[-2].tag == JS_TAG_INT */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);
    not_int2 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Both are integers: load int32 values */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);

    /* Perform the bitwise operation */
    sljit_emit_op2(C, sljit_op, SLJIT_R0, 0,
                   SLJIT_R0, 0, SLJIT_R1, 0);

    /* Store result to sp[-2].u.int32 (tag stays JS_TAG_INT) */
    sljit_emit_op1(C, SLJIT_MOV_S32,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    /* sp-- */
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* Slow path: call jit_op_binary_logic(ctx, aux, opcode) */
    {
        struct sljit_label *slow_label = sljit_emit_label(C);
        sljit_set_label(not_int1, slow_label);
        sljit_set_label(not_int2, slow_label);
    }
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                   REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, opcode);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS3(W, P, P, W),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_op_binary_logic));
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    {
        struct sljit_jump *exc_jump = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);
        exc_jumps[(*n_exc_jumps)++] = exc_jump;
    }

    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * emit_return_fast: Inline return — store sp[-1] to aux->ret_val,
 * set aux->sp = sp - 1, return 0.  No helper call needed.
 */
static void emit_return_fast(struct sljit_compiler *C)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX),
                   (sljit_sw)offsetof(JitAux, ret_val) + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX),
                   (sljit_sw)offsetof(JitAux, ret_val) + JSV_TAG_OFF,
                   SLJIT_R1, 0);
    sljit_emit_op2(C, SLJIT_SUB, SLJIT_R0, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                   SLJIT_R0, 0);
    sljit_emit_return(C, SLJIT_MOV, SLJIT_IMM, 0);
}

/*
 * emit_return_undef_fast: Inline return_undef — store JS_UNDEFINED
 * to aux->ret_val, set aux->sp = sp, return 0.
 */
static void emit_return_undef_fast(struct sljit_compiler *C)
{
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX),
                   (sljit_sw)offsetof(JitAux, ret_val) + JSV_U_OFF,
                   SLJIT_IMM, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX),
                   (sljit_sw)offsetof(JitAux, ret_val) + JSV_TAG_OFF,
                   SLJIT_IMM, JS_TAG_UNDEFINED);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                   REG_SP, 0);
    sljit_emit_return(C, SLJIT_MOV, SLJIT_IMM, 0);
}

static void emit_eq_fast(struct sljit_compiler *C,
                          int opcode,
                          struct sljit_jump **exc_jumps,
                          int *n_exc_jumps)
{
    struct sljit_jump *not_int1, *not_int2, *done;
    sljit_s32 cond;
    int is_eq = (opcode == OP_eq || opcode == OP_strict_eq);
    int is_strict = (opcode == OP_strict_eq || opcode == OP_strict_neq);
    cond = is_eq ? SLJIT_EQUAL : SLJIT_NOT_EQUAL;

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    not_int1 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);
    not_int2 = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    /* Both int: compare values branchlessly */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);

    sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_R1, 0);
    sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_R0, 0, cond);

    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_IMM, JS_TAG_BOOL);
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* Not both int */
    {
        struct sljit_label *not_int_label = sljit_emit_label(C);
        sljit_set_label(not_int1, not_int_label);
        sljit_set_label(not_int2, not_int_label);
    }

    if (!is_strict) {
        /* Non-strict ==: inline nullish comparison fast path.
         * If at least one operand is null/undefined and neither is refcounted,
         * resolve inline. Otherwise call C helper. */
        struct sljit_jump *any_refcounted, *tag1_is_nullish, *tag2_is_nullish;
        struct sljit_jump *neither_nullish, *done_nullish1, *done_nullish2;

        /* R0 = tag1, R1 = tag2 */
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                       SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                       SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);

        /* Either refcounted → slow */
        sljit_emit_op2(C, SLJIT_OR, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        any_refcounted = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                                         SLJIT_R2, 0, SLJIT_IMM, 0);

        /* (tag-2) unsigned <= 1 detects null(2) and undefined(3) */
        sljit_emit_op2(C, SLJIT_SUB, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_IMM, JS_TAG_NULL);
        tag1_is_nullish = sljit_emit_cmp(C, SLJIT_LESS_EQUAL,
                                          SLJIT_R2, 0, SLJIT_IMM, 1);

        sljit_emit_op2(C, SLJIT_SUB, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, JS_TAG_NULL);
        tag2_is_nullish = sljit_emit_cmp(C, SLJIT_LESS_EQUAL,
                                          SLJIT_R2, 0, SLJIT_IMM, 1);

        /* Neither nullish, both non-refcounted (e.g. bool==int) → slow path */
        neither_nullish = sljit_emit_jump(C, SLJIT_JUMP);

        /* tag2 nullish, tag1 not → false for == (null != int/bool) */
        sljit_set_label(tag2_is_nullish, sljit_emit_label(C));
        sljit_emit_op1(C, SLJIT_MOV,
                       SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                       SLJIT_IMM, is_eq ? 0 : 1);
        sljit_emit_op1(C, SLJIT_MOV,
                       SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF,
                       SLJIT_IMM, JS_TAG_BOOL);
        sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                       SLJIT_IMM, JSV_SIZE);
        done_nullish1 = sljit_emit_jump(C, SLJIT_JUMP);

        /* tag1 nullish: result = (tag2 also nullish) ? eq : !eq */
        sljit_set_label(tag1_is_nullish, sljit_emit_label(C));
        sljit_emit_op2(C, SLJIT_SUB, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, JS_TAG_NULL);
        sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_LESS_EQUAL,
                        SLJIT_R2, 0, SLJIT_IMM, 1);
        sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_R0, 0,
                            is_eq ? SLJIT_LESS_EQUAL : SLJIT_GREATER);

        sljit_emit_op1(C, SLJIT_MOV,
                       SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                       SLJIT_R0, 0);
        sljit_emit_op1(C, SLJIT_MOV,
                       SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF,
                       SLJIT_IMM, JS_TAG_BOOL);
        sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                       SLJIT_IMM, JSV_SIZE);
        done_nullish2 = sljit_emit_jump(C, SLJIT_JUMP);

        /* Slow path: refcounted or non-nullish non-int types */
        {
            struct sljit_label *slow_label = sljit_emit_label(C);
            sljit_set_label(any_refcounted, slow_label);
            sljit_set_label(neither_nullish, slow_label);
        }
        sljit_emit_op1(C, SLJIT_MOV,
                       SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                       REG_SP, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, opcode);
        sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS3(W, P, P, W),
                         SLJIT_IMM, (sljit_sw)(void *)jit_op_eq);
        sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                       SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
        exc_jumps[(*n_exc_jumps)++] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);

        {
            struct sljit_label *end_label = sljit_emit_label(C);
            sljit_set_label(done, end_label);
            sljit_set_label(done_nullish1, end_label);
            sljit_set_label(done_nullish2, end_label);
        }
    } else {
        /* Strict eq/neq: just call helper */
        sljit_emit_op1(C, SLJIT_MOV,
                       SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                       REG_SP, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, opcode);
        sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS3(W, P, P, W),
                         SLJIT_IMM, (sljit_sw)(void *)jit_op_strict_eq);
        sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                       SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
        exc_jumps[(*n_exc_jumps)++] = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);
        sljit_set_label(done, sljit_emit_label(C));
    }
}

static void emit_inc_dec_fast(struct sljit_compiler *C,
                               int is_dec,
                               struct sljit_jump **exc_jumps,
                               int *n_exc_jumps)
{
    struct sljit_jump *not_int, *overflow, *done;

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    not_int = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                             SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);

    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);

    sljit_emit_op2(C, (is_dec ? SLJIT_SUB : SLJIT_ADD) | SLJIT_SET_OVERFLOW,
                   SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
    overflow = sljit_emit_jump(C, SLJIT_OVERFLOW);

    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    {
        struct sljit_label *slow_label = sljit_emit_label(C);
        sljit_set_label(not_int, slow_label);
        sljit_set_label(overflow, slow_label);
    }
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                   REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, (sljit_sw)(is_dec ? (void *)jit_op_dec : (void *)jit_op_inc));
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    {
        struct sljit_jump *exc_jump = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);
        exc_jumps[(*n_exc_jumps)++] = exc_jump;
    }

    sljit_set_label(done, sljit_emit_label(C));
}

static void emit_push_this_fast(struct sljit_compiler *C,
                                 struct sljit_jump **exc_jumps,
                                 int *n_exc_jumps)
{
    struct sljit_jump *not_object, *done;

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_AUX),
                   (sljit_sw)offsetof(JitAux, this_obj) + JSV_TAG_OFF);
    not_object = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                SLJIT_R0, 0, SLJIT_IMM, JS_TAG_OBJECT);

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_AUX),
                   (sljit_sw)offsetof(JitAux, this_obj) + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), JSV_TAG_OFF,
                   SLJIT_IMM, JS_TAG_OBJECT);

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R2, 0,
                   SLJIT_MEM1(SLJIT_R1), 0);
    sljit_emit_op2(C, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
    sljit_emit_op1(C, SLJIT_MOV32,
                   SLJIT_MEM1(SLJIT_R1), 0,
                   SLJIT_R2, 0);

    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    sljit_set_label(not_object, sljit_emit_label(C));
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                   REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_op_push_this));
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    {
        struct sljit_jump *exc_jump = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);
        exc_jumps[(*n_exc_jumps)++] = exc_jump;
    }

    sljit_set_label(done, sljit_emit_label(C));
}

/* ---- Inline logical-not fast path ----
 * If tag <= JS_TAG_UNDEFINED (INT, BOOL, NULL, UNDEFINED):
 *   result = !(int_val != 0) → written as js_bool(!val)
 * Else: fall back to jit_op_lnot (handles strings, objects, floats)
 * Never changes sp.
 */
static void emit_lnot_fast(struct sljit_compiler *C,
                            struct sljit_jump **exc_jumps,
                            int *n_exc_jumps)
{
    struct sljit_jump *slow_jump, *done;

    /* Load tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);

    /* tag > JS_TAG_UNDEFINED (3) → slow path (unsigned compare: tags 0-3 are fast) */
    slow_jump = sljit_emit_cmp(C, SLJIT_GREATER,
                               SLJIT_R0, 0, SLJIT_IMM, JS_TAG_UNDEFINED);

    /* Fast path: load int32 value */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);

    /* result = (val == 0) ? 1 : 0   i.e. !val */
    sljit_emit_op2u(C, SLJIT_SUB | SLJIT_SET_Z,
                    SLJIT_R0, 0, SLJIT_IMM, 0);
    sljit_emit_op_flags(C, SLJIT_MOV, SLJIT_R0, 0, SLJIT_EQUAL);

    /* Write result as js_bool */
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_IMM, JS_TAG_BOOL);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* Slow path: call C helper */
    sljit_set_label(slow_jump, sljit_emit_label(C));
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                   REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_op_lnot));
    /* lnot always returns 0 — no exception check needed,
       but sp is still in aux->sp, reload is needed since helper wrote it */
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));

    sljit_set_label(done, sljit_emit_label(C));
    (void)exc_jumps;
    (void)n_exc_jumps;
}

/* ---- Inline to_propkey fast path ----
 * If tag is INT, STRING, or SYMBOL: no-op (already valid property key).
 * Else: fall back to jit_op_to_propkey.
 * ~90% of property keys are already one of these types.
 */
static void emit_to_propkey_fast(struct sljit_compiler *C,
                                  struct sljit_jump **exc_jumps,
                                  int *n_exc_jumps)
{
    struct sljit_jump *done_int, *done_str, *done_sym;

    /* Load tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);

    /* Check INT (tag == 0) */
    done_int = sljit_emit_cmp(C, SLJIT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_INT);
    /* Check STRING (tag == -7) */
    done_str = sljit_emit_cmp(C, SLJIT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_STRING);
    /* Check SYMBOL (tag == -8) */
    done_sym = sljit_emit_cmp(C, SLJIT_EQUAL,
                              SLJIT_R0, 0, SLJIT_IMM, JS_TAG_SYMBOL);

    /* Slow path: needs conversion */
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                   REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_op_to_propkey));
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    {
        struct sljit_jump *exc_jump = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                      SLJIT_RETURN_REG, 0,
                                                      SLJIT_IMM, 0);
        exc_jumps[(*n_exc_jumps)++] = exc_jump;
    }

    /* All fast paths land here (no-op) */
    {
        struct sljit_label *done_label = sljit_emit_label(C);
        sljit_set_label(done_int, done_label);
        sljit_set_label(done_str, done_label);
        sljit_set_label(done_sym, done_label);
    }
}

/* ---- Inline insert3 fast path ----
 * insert3: a b c → c a b c  (dup TOS, insert copy 3 deep)
 * sp[0] = sp[-1]; sp[-1] = sp[-2]; sp[-2] = sp[-3];
 * sp[-3] = JS_DupValue(ctx, sp[0]); sp++;
 *
 * Fast path: if TOS tag >= 0 (non-refcounted: INT, BOOL, NULL, UNDEFINED, FLOAT64),
 * just copy memory. Otherwise inline refcount bump.
 */
static void emit_insert3_fast(struct sljit_compiler *C)
{
    struct sljit_jump *not_refcounted;

    /* Load TOS value (sp[-1]) into R0(val), R1(tag) */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);

    /* Shift stack: sp[-1] = sp[-2] */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF,
                   SLJIT_R2, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_R3, 0);

    /* sp[-2] = sp[-3] */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM1(REG_SP), -3 * JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0,
                   SLJIT_MEM1(REG_SP), -3 * JSV_SIZE + JSV_TAG_OFF);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                   SLJIT_R2, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_R3, 0);

    /* sp[-3] = original TOS (R0/R1) — the dup target */
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -3 * JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -3 * JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_R1, 0);

    /* Also write sp[0] = original TOS (this is the new slot pushed) */
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), JSV_TAG_OFF,
                   SLJIT_R1, 0);

    /* JS_DupValue: if tag >= 0, non-refcounted → done.
       if tag < 0, refcounted → bump ref_count at offset 0 of ptr. */
    not_refcounted = sljit_emit_cmp(C, SLJIT_SIG_GREATER_EQUAL,
                                    SLJIT_R1, 0, SLJIT_IMM, 0);

    /* Refcounted: R0 is the object pointer. ref_count is int32 at offset 0. */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R2, 0,
                   SLJIT_MEM1(SLJIT_R0), 0);
    sljit_emit_op2(C, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
    sljit_emit_op1(C, SLJIT_MOV32,
                   SLJIT_MEM1(SLJIT_R0), 0,
                   SLJIT_R2, 0);

    sljit_set_label(not_refcounted, sljit_emit_label(C));

    /* sp++ */
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0, SLJIT_IMM, JSV_SIZE);
}

/* ---- Inline insert2 fast path ----
 * insert2: a b → b a b  (dup TOS, insert copy 2 deep)
 * sp[0] = sp[-1]; sp[-1] = sp[-2]; sp[-2] = JS_DupValue(ctx, sp[0]); sp++;
 */
static void emit_insert2_fast(struct sljit_compiler *C)
{
    struct sljit_jump *not_refcounted;

    /* Load TOS (sp[-1]) into R0(val), R1(tag) */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);

    /* sp[-1] = sp[-2] */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF,
                   SLJIT_R2, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_R3, 0);

    /* sp[-2] = original TOS (R0/R1) — the dup target */
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_R1, 0);

    /* sp[0] = original TOS (new pushed slot) */
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), JSV_TAG_OFF,
                   SLJIT_R1, 0);

    /* JS_DupValue inline: tag < 0 → refcounted → bump ref_count */
    not_refcounted = sljit_emit_cmp(C, SLJIT_SIG_GREATER_EQUAL,
                                    SLJIT_R1, 0, SLJIT_IMM, 0);

    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R2, 0,
                   SLJIT_MEM1(SLJIT_R0), 0);
    sljit_emit_op2(C, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
    sljit_emit_op1(C, SLJIT_MOV32,
                   SLJIT_MEM1(SLJIT_R0), 0,
                   SLJIT_R2, 0);

    sljit_set_label(not_refcounted, sljit_emit_label(C));

    /* sp++ */
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0, SLJIT_IMM, JSV_SIZE);
}

#endif /* struct mode guards */

/*
 * Dispatch macros: use fast-path versions in struct mode,
 * fall back to original helpers in NaN-boxing/CHECK modes.
 */
#if !(defined(JS_NAN_BOXING) && JS_NAN_BOXING) && !defined(JS_CHECK_JSVALUE)
#define EMIT_GET_VAR(C, buf, idx)       emit_get_var_fast(C, buf, idx)
#define EMIT_PUT_VAR(C, buf, idx)       emit_put_var_fast(C, buf, idx)
#define EMIT_DROP(C)                    emit_drop_fast(C)
#define EMIT_BRANCH(C, sense)           emit_branch_fast(C, sense)
#define EMIT_ADD(C, ej, nej)            emit_add_sub_fast(C, 0, ej, nej)
#define EMIT_SUB(C, ej, nej)            emit_add_sub_fast(C, 1, ej, nej)
#define EMIT_MUL(C, ej, nej)            emit_mul_fast(C, ej, nej)
#define EMIT_SWAP(C)                    emit_swap_fast(C)
#define EMIT_DUP(C)                     emit_dup_fast(C)
#define EMIT_NIP(C)                     emit_nip_fast(C)
#define EMIT_RELATIONAL(C, op, ej, nej) emit_relational_fast(C, op, ej, nej)
#define EMIT_BINARY_LOGIC(C, op, ej, nej) emit_binary_logic_fast(C, op, ej, nej)
#define EMIT_RETURN(C)                  emit_return_fast(C)
#define EMIT_RETURN_UNDEF(C)            emit_return_undef_fast(C)
#define EMIT_EQ(C, op, ej, nej)         emit_eq_fast(C, op, ej, nej)
#define EMIT_INC_DEC(C, is_dec, ej, nej) emit_inc_dec_fast(C, is_dec, ej, nej)
#define EMIT_PUSH_THIS(C, ej, nej)      emit_push_this_fast(C, ej, nej)
#define EMIT_LNOT(C, ej, nej)          emit_lnot_fast(C, ej, nej)
#define EMIT_TO_PROPKEY(C, ej, nej)    emit_to_propkey_fast(C, ej, nej)
#define EMIT_INSERT3(C)                emit_insert3_fast(C)
#define EMIT_INSERT2(C)                emit_insert2_fast(C)
#define EMIT_IS_UNDEF_OR_NULL(C, ej, nej) emit_is_undefined_or_null_fast(C, ej, nej)
#else
#define EMIT_GET_VAR(C, buf, idx)       emit_get_var(C, buf, idx)
#define EMIT_PUT_VAR(C, buf, idx)       emit_put_var(C, buf, idx)
#define EMIT_DROP(C)                    emit_drop(C)
#define EMIT_BRANCH(C, sense)           emit_branch(C, sense)
#define EMIT_ADD(C, ej, nej)            do { struct sljit_jump *_j = emit_arith(C, (void *)qjs_jit_add, NULL); (ej)[(*nej)++] = _j; } while(0)
#define EMIT_SUB(C, ej, nej)            do { struct sljit_jump *_j = emit_arith(C, (void *)qjs_jit_sub, NULL); (ej)[(*nej)++] = _j; } while(0)
#define EMIT_MUL(C, ej, nej)            do { struct sljit_jump *_j = emit_arith(C, (void *)qjs_jit_mul, NULL); (ej)[(*nej)++] = _j; } while(0)
#define EMIT_SWAP(C)                    emit_swap(C)
#define EMIT_DUP(C)                     emit_dup(C)
#define EMIT_NIP(C)                     emit_nip(C)
#define EMIT_RELATIONAL(C, op, ej, nej) do { (ej)[(*nej)++] = emit_op_call_int(C, (void *)jit_op_relational, op); } while(0)
#define EMIT_BINARY_LOGIC(C, op, ej, nej) do { (ej)[(*nej)++] = emit_op_call_int(C, (void *)jit_op_binary_logic, op); } while(0)
#define EMIT_RETURN(C)                  emit_return(C)
#define EMIT_RETURN_UNDEF(C)            emit_return_undef(C)
#define EMIT_EQ(C, op, ej, nej)         do { (ej)[(*nej)++] = emit_op_call_int(C, (void *)((op == OP_strict_eq || op == OP_strict_neq) ? (void *)jit_op_strict_eq : (void *)jit_op_eq), op); } while(0)
#define EMIT_INC_DEC(C, is_dec, ej, nej) do { (ej)[(*nej)++] = emit_op_call_2(C, (is_dec) ? (void *)jit_op_dec : (void *)jit_op_inc); } while(0)
#define EMIT_PUSH_THIS(C, ej, nej)      do { (ej)[(*nej)++] = emit_op_call_2(C, (void *)jit_op_push_this); } while(0)
#define EMIT_LNOT(C, ej, nej)          do { (ej)[(*nej)++] = emit_op_call_2(C, (void *)jit_op_lnot); } while(0)
#define EMIT_TO_PROPKEY(C, ej, nej)    do { (ej)[(*nej)++] = emit_op_call_2(C, (void *)jit_op_to_propkey); } while(0)
#define EMIT_INSERT3(C)                do { (void)emit_op_call_2(C, (void *)jit_op_insert3); } while(0)
#define EMIT_INSERT2(C)                do { (void)emit_op_call_2(C, (void *)jit_op_insert2); } while(0)
#define EMIT_IS_UNDEF_OR_NULL(C, ej, nej) do { (ej)[(*nej)++] = emit_op_call_2(C, (void *)jit_op_is_undefined_or_null); } while(0)
#endif

/* Emit a binary arithmetic op via extern helper (add/sub/mul).
 * Calls qjs_jit_add/sub/mul(ctx, sp). Returns 0 ok, -1 exception.
 * On success sp[-2] = result. Caller decrements sp. */
#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)
static struct sljit_jump *emit_arith(struct sljit_compiler *C,
                                     void *helper_fn,
                                     struct sljit_label **exc_label)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(helper_fn));
    /* Check return value: if non-zero, jump to exception */
    struct sljit_jump *exc_jump = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                                  SLJIT_RETURN_REG, 0,
                                                  SLJIT_IMM, 0);
    /* sp-- on success */
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    return exc_jump;
}
#endif

#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)
static void emit_return(struct sljit_compiler *C)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_AUX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_return));
    sljit_emit_return(C, SLJIT_MOV, SLJIT_IMM, 0);
}

/* Emit return_undef: call jit_helper_return_undef(aux, sp), return 0 */
static void emit_return_undef(struct sljit_compiler *C)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_AUX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2V(P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_return_undef));
    sljit_emit_return(C, SLJIT_MOV, SLJIT_IMM, 0);
}
#endif

/*
 * Emit generator suspend: save sp + suspend_code into aux, then return 2.
 * suspend_code: 0=AWAIT, 1=YIELD, 2=YIELD_STAR, 3=UNDEFINED (initial_yield/return_async)
 * resume_pc: bytecode pointer where the interpreter should resume.
 */
static void emit_generator_suspend(struct sljit_compiler *C, sljit_sw suspend_code,
                                    const uint8_t *resume_pc)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_AUX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, suspend_code);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, (sljit_sw)resume_pc);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS4V(P, P, W, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_generator_suspend));
    sljit_emit_return(C, SLJIT_MOV, SLJIT_IMM, 2);
}

/* Emit if_false/if_true branch. Calls jit_helper_to_bool_free(ctx, sp),
 * decrements sp, then branches based on result.
 * sense=0: branch if false (SLJIT_EQUAL to 0)
 * sense=1: branch if true  (SLJIT_NOT_EQUAL to 0) */
#if (defined(JS_NAN_BOXING) && JS_NAN_BOXING) || defined(JS_CHECK_JSVALUE)
static struct sljit_jump *emit_branch(struct sljit_compiler *C, int sense)
{
    /* Call to_bool_free(ctx, sp) */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_SP, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P),
                     SLJIT_IMM, SLJIT_FUNC_ADDR(jit_helper_to_bool_free));
    /* sp-- */
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, JSV_SIZE);
    /* Branch based on result */
    return sljit_emit_cmp(C,
                          sense ? SLJIT_NOT_EQUAL : SLJIT_EQUAL,
                          SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);
}
#endif

/* Emit icall to func(ctx, aux). Saves/reloads sp/vbuf/abuf.
 * Returns exc_jump (return != 0 means exception).
 * Note: helpers return int (32-bit). On x64 MSVC, `mov eax, -1` zero-extends
 * to 0x00000000FFFFFFFF which is positive for SLJIT_SIG_LESS. Use NOT_EQUAL. */
static struct sljit_jump *emit_op_call_2(struct sljit_compiler *C, void *func)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp), REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(W, P, P), SLJIT_IMM, SLJIT_FUNC_ADDR(func));
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0, SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    return sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);
}

/* Emit icall to func(ctx, aux, pc). pc is compile-time constant pointer. */
static struct sljit_jump *emit_op_call_pc(struct sljit_compiler *C, void *func, const uint8_t *pc)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp), REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)pc);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS3(W, P, P, P), SLJIT_IMM, SLJIT_FUNC_ADDR(func));
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0, SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    return sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);
}

/* Emit icall to func(ctx, aux, int_param). */
static struct sljit_jump *emit_op_call_int(struct sljit_compiler *C, void *func, sljit_sw param)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp), REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, param);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS3(W, P, P, W), SLJIT_IMM, SLJIT_FUNC_ADDR(func));
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0, SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    return sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);
}

/* Emit icall to func(ctx, aux, pc, ic). pc and ic are compile-time constants. */
static struct sljit_jump *emit_op_call_pc_ic(struct sljit_compiler *C, void *func,
                                              const uint8_t *pc, PropIC *ic)
{
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp), REG_SP, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)pc);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, (sljit_sw)ic);
    sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS4(W, P, P, P, P), SLJIT_IMM, SLJIT_FUNC_ADDR(func));
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0, SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    return sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_RETURN_REG, 0, SLJIT_IMM, 0);
}

static void emit_is_undefined_or_null_fast(struct sljit_compiler *C,
                                            struct sljit_jump **exc_jumps,
                                            int *n_exc_jumps)
{
    struct sljit_jump *is_refcounted, *is_nullish, *done_false, *done_true;

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);

    is_refcounted = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                                    SLJIT_R0, 0, SLJIT_IMM, 0);

    /* (tag - 2) unsigned <= 1 matches null(2) and undefined(3) */
    sljit_emit_op2(C, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0,
                   SLJIT_IMM, JS_TAG_NULL);
    is_nullish = sljit_emit_cmp(C, SLJIT_LESS_EQUAL,
                                 SLJIT_R0, 0, SLJIT_IMM, 1);

    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF,
                   SLJIT_IMM, 0);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_IMM, JS_TAG_BOOL);
    done_false = sljit_emit_jump(C, SLJIT_JUMP);

    sljit_set_label(is_nullish, sljit_emit_label(C));
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF,
                   SLJIT_IMM, 1);
    sljit_emit_op1(C, SLJIT_MOV,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_IMM, JS_TAG_BOOL);
    done_true = sljit_emit_jump(C, SLJIT_JUMP);

    {
        struct sljit_label *slow_label = sljit_emit_label(C);
        sljit_set_label(is_refcounted, slow_label);
        exc_jumps[(*n_exc_jumps)++] = emit_op_call_2(C, (void *)jit_op_is_undefined_or_null);
    }

    {
        struct sljit_label *end_label = sljit_emit_label(C);
        sljit_set_label(done_false, end_label);
        sljit_set_label(done_true, end_label);
    }
}

/*
 * Inline IC fast path for OP_get_field.
 * On IC hit with obj ref_count > 1: fully inline (no function call).
 * On miss/not-object/ref_count<=1: falls through to C helper.
 * Uses only R0-R2 scratch registers (portable to all SLJIT targets).
 */
static void emit_get_field_ic_fast(struct sljit_compiler *C,
                                    const uint8_t *pc, PropIC *ic,
                                    const JitICLayout *layout,
                                    struct sljit_jump **exc_jumps,
                                    int *n_exc_jumps)
{
    struct sljit_jump *not_object, *ic_miss, *ref_low, *not_refcounted, *done;
    struct sljit_label *slow_label;

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    not_object = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                SLJIT_R0, 0, SLJIT_IMM, JS_TAG_OBJECT);

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(SLJIT_R0), layout->obj_shape_off);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM0(), (sljit_sw)&ic->cached_shape);
    ic_miss = sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_R2, 0);

    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0,
                   SLJIT_MEM1(SLJIT_R0), 0);
    ref_low = sljit_emit_cmp(C, SLJIT_SIG_LESS_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 1);

    sljit_emit_op2(C, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
    sljit_emit_op1(C, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_R1, 0);

    sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R1, 0,
                   SLJIT_MEM0(), (sljit_sw)&ic->cached_offset);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM1(SLJIT_R0), layout->obj_prop_off);
    sljit_emit_op2(C, SLJIT_MUL, SLJIT_R1, 0, SLJIT_R1, 0,
                   SLJIT_IMM, layout->prop_size);
    sljit_emit_op2(C, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_R1, 0);

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(SLJIT_R2), JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(SLJIT_R2), JSV_TAG_OFF);

    not_refcounted = sljit_emit_cmp(C, SLJIT_SIG_GREATER_EQUAL,
                                     SLJIT_R1, 0, SLJIT_IMM, 0);
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), 0);
    sljit_emit_op2(C, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
    sljit_emit_op1(C, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_R2, 0);
    sljit_set_label(not_refcounted, sljit_emit_label(C));

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF,
                   SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF,
                   SLJIT_R1, 0);

    done = sljit_emit_jump(C, SLJIT_JUMP);

    slow_label = sljit_emit_label(C);
    sljit_set_label(not_object, slow_label);
    sljit_set_label(ic_miss, slow_label);
    sljit_set_label(ref_low, slow_label);
    exc_jumps[(*n_exc_jumps)++] = emit_op_call_pc_ic(C,
        (void *)jit_op_get_field_ic, pc, ic);

    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * Inline IC fast path for OP_get_field2.
 * Pushes value WITHOUT consuming the object (sp grows by 1).
 * No ref_count check needed for old obj since it's not freed.
 */
static void emit_get_field2_ic_fast(struct sljit_compiler *C,
                                     const uint8_t *pc, PropIC *ic,
                                     const JitICLayout *layout,
                                     struct sljit_jump **exc_jumps,
                                     int *n_exc_jumps)
{
    struct sljit_jump *not_object, *ic_miss, *not_refcounted, *done;
    struct sljit_label *slow_label;

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    not_object = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                SLJIT_R0, 0, SLJIT_IMM, JS_TAG_OBJECT);

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(SLJIT_R0), layout->obj_shape_off);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM0(), (sljit_sw)&ic->cached_shape);
    ic_miss = sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_R2, 0);

    sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R1, 0,
                   SLJIT_MEM0(), (sljit_sw)&ic->cached_offset);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM1(SLJIT_R0), layout->obj_prop_off);
    sljit_emit_op2(C, SLJIT_MUL, SLJIT_R1, 0, SLJIT_R1, 0,
                   SLJIT_IMM, layout->prop_size);
    sljit_emit_op2(C, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_R1, 0);

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(SLJIT_R2), JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(SLJIT_R2), JSV_TAG_OFF);

    not_refcounted = sljit_emit_cmp(C, SLJIT_SIG_GREATER_EQUAL,
                                     SLJIT_R1, 0, SLJIT_IMM, 0);
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), 0);
    sljit_emit_op2(C, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
    sljit_emit_op1(C, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_R2, 0);
    sljit_set_label(not_refcounted, sljit_emit_label(C));

    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), JSV_U_OFF, SLJIT_R0, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_SP), JSV_TAG_OFF, SLJIT_R1, 0);
    sljit_emit_op2(C, SLJIT_ADD, REG_SP, 0, REG_SP, 0, SLJIT_IMM, JSV_SIZE);

    done = sljit_emit_jump(C, SLJIT_JUMP);

    slow_label = sljit_emit_label(C);
    sljit_set_label(not_object, slow_label);
    sljit_set_label(ic_miss, slow_label);
    exc_jumps[(*n_exc_jumps)++] = emit_op_call_pc_ic(C,
        (void *)jit_op_get_field2_ic, pc, ic);

    sljit_set_label(done, sljit_emit_label(C));
}

/*
 * Inline IC fast path for OP_put_field.
 * Fully inlines the IC-hit path when:
 *   1. sp[-2] is an object with matching shape
 *   2. Old property value is non-refcounted (tag >= 0)
 *   3. New value (sp[-1]) is non-refcounted (tag >= 0)
 * When both old and new are non-refcounted (int/bool/null/undefined),
 * the entire put_field completes with zero function calls.
 * Otherwise falls to the C helper for ref-counting.
 * Uses only R0-R2 (portable to all SLJIT targets).
 */
static void emit_put_field_ic_fast(struct sljit_compiler *C,
                                    const uint8_t *pc, PropIC *ic,
                                    const JitICLayout *layout,
                                    struct sljit_jump **exc_jumps,
                                    int *n_exc_jumps)
{
    struct sljit_jump *not_object, *ic_miss, *old_refcounted, *new_refcounted;
    struct sljit_jump *obj_ref_low, *done;
    struct sljit_label *slow_label;

    /* 1. Check sp[-2].tag == JS_TAG_OBJECT */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_TAG_OFF);
    not_object = sljit_emit_cmp(C, SLJIT_NOT_EQUAL,
                                SLJIT_R0, 0, SLJIT_IMM, JS_TAG_OBJECT);

    /* 2. Load obj ptr, check shape match */
    /* R0 = obj ptr */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                   SLJIT_MEM1(REG_SP), -2 * JSV_SIZE + JSV_U_OFF);
    /* R1 = obj->shape */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(SLJIT_R0), layout->obj_shape_off);
    /* R2 = ic->cached_shape */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM0(), (sljit_sw)&ic->cached_shape);
    ic_miss = sljit_emit_cmp(C, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_R2, 0);

    /* 3. Compute property address: R1 = &prop[cached_offset] */
    /* R2 = ic->cached_offset */
    sljit_emit_op1(C, SLJIT_MOV_U32, SLJIT_R2, 0,
                   SLJIT_MEM0(), (sljit_sw)&ic->cached_offset);
    /* R1 = obj->prop */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0,
                   SLJIT_MEM1(SLJIT_R0), layout->obj_prop_off);
    /* R2 = cached_offset * prop_size */
    sljit_emit_op2(C, SLJIT_MUL, SLJIT_R2, 0, SLJIT_R2, 0,
                   SLJIT_IMM, layout->prop_size);
    /* R1 = &prop[cached_offset] (property address) */
    sljit_emit_op2(C, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);

    /* 4. Check old property value tag: if refcounted (tag < 0), go to helper */
    /* R2 = old_val.tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM1(SLJIT_R1), JSV_TAG_OFF);
    old_refcounted = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                                     SLJIT_R2, 0, SLJIT_IMM, 0);

    /* 5. Check new value (sp[-1]) tag: if refcounted, go to helper */
    /* R2 = new_val.tag */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_TAG_OFF);
    new_refcounted = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                                     SLJIT_R2, 0, SLJIT_IMM, 0);

    /* 6. FAST PATH: both old and new are non-refcounted.
     *    Store new value (u + tag) into property slot. */
    /* R2 already has new_val.tag, load new_val.u */
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), JSV_TAG_OFF,
                   SLJIT_R2, 0);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0,
                   SLJIT_MEM1(REG_SP), -JSV_SIZE + JSV_U_OFF);
    sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), JSV_U_OFF,
                   SLJIT_R2, 0);

    /* 7. Decrement obj->ref_count (R0 = obj ptr).
     *    If ref_count would reach 0, go to slow path.
     *    (We already checked shape match, so obj is valid.) */
    sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_R1, 0,
                   SLJIT_MEM1(SLJIT_R0), 0);
    obj_ref_low = sljit_emit_cmp(C, SLJIT_SIG_LESS_EQUAL,
                                  SLJIT_R1, 0, SLJIT_IMM, 1);
    sljit_emit_op2(C, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
    sljit_emit_op1(C, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_R1, 0);

    /* 8. sp -= 2 */
    sljit_emit_op2(C, SLJIT_SUB, REG_SP, 0, REG_SP, 0,
                   SLJIT_IMM, 2 * JSV_SIZE);
    done = sljit_emit_jump(C, SLJIT_JUMP);

    /* SLOW PATH: call jit_op_put_field_ic_hit for IC hit but refcounted values,
     * or jit_op_put_field_ic for IC miss / not object / obj about to be freed */
    {
        struct sljit_jump *done2;
        struct sljit_label *hit_slow;

        /* IC hit but needs ref-counting: call jit_op_put_field_ic_hit(ctx, aux, ic) */
        hit_slow = sljit_emit_label(C);
        sljit_set_label(old_refcounted, hit_slow);
        sljit_set_label(new_refcounted, hit_slow);
        sljit_set_label(obj_ref_low, hit_slow);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_MEM1(REG_AUX),
                       (sljit_sw)offsetof(JitAux, sp), REG_SP, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM, (sljit_sw)ic);
        sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS3(W, P, P, P),
                         SLJIT_IMM, SLJIT_FUNC_ADDR(jit_op_put_field_ic_hit));
        sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                       SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
        done2 = sljit_emit_jump(C, SLJIT_JUMP);

        /* IC miss / not object: call full jit_op_put_field_ic(ctx, aux, pc, ic) */
        slow_label = sljit_emit_label(C);
        sljit_set_label(not_object, slow_label);
        sljit_set_label(ic_miss, slow_label);
        exc_jumps[(*n_exc_jumps)++] = emit_op_call_pc_ic(C,
            (void *)jit_op_put_field_ic, pc, ic);

        {
            struct sljit_label *end_label = sljit_emit_label(C);
            sljit_set_label(done, end_label);
            sljit_set_label(done2, end_label);
        }
    }
}


/* ---- Bytecode analysis ---- */

/* Check if a function's bytecode can be JIT compiled (blacklist approach).
 * Returns 1 if no blacklisted opcodes are found, 0 otherwise.
 * If first_unsupported is non-NULL, stores the first unsupported opcode. */
static int can_jit_compile(const uint8_t *bc, int bc_len,
                           int *first_unsupported)
{
    int pos = 0;

    if (first_unsupported)
        *first_unsupported = -1;

    while (pos < bc_len) {
        uint8_t op = bc[pos];
        if (op >= OP_COUNT) {
            if (first_unsupported) *first_unsupported = op;
            return 0; /* temporary or invalid opcode */
        }

        switch (op) {
        /* No blacklisted opcodes currently */
        default:
            break;
        }

        pos += jit_opcode_size[op];
    }
    return 1;
}

/* ---- Main JIT compilation ---- */

void js_sljit_compile(JSContext *ctx,
                      uint8_t *byte_code_buf, int byte_code_len,
                      int arg_count, int var_count, int stack_size,
                      JitFunc *out_jitcode, void **out_jit_code_ptr,
                      JitDispatchEntry **out_dispatch_table,
                      int *out_dispatch_count,
                      PropIC **out_ic_cache,
                      int *out_ic_count)
{
    struct sljit_compiler *C;
    struct sljit_label **labels = NULL;
    uint8_t *is_target = NULL;
    JitJumpPatch *deferred = NULL;
    struct sljit_jump **exc_jumps = NULL;
    int n_deferred = 0, max_deferred;
    int n_exc_jumps = 0, max_exc_jumps;
    int pos, bc_len;
    const uint8_t *bc;
    void *code;
    JitDispatchEntry *dispatch_table = NULL;
    int n_dispatch = 0, max_dispatch;
    PropIC *ic_array = NULL;
    int ic_total = 0, ic_idx = 0;
    JitICLayout ic_layout;

    *out_jitcode = NULL;
    *out_jit_code_ptr = NULL;
    *out_dispatch_table = NULL;
    *out_dispatch_count = 0;
    *out_ic_cache = NULL;
    *out_ic_count = 0;

    bc = byte_code_buf;
    bc_len = byte_code_len;
    if (bc_len <= 0)
        return;

    /* Phase 0: check if function can be JIT compiled */
    {
        int unsupported_op = -1;
        if (!can_jit_compile(bc, bc_len, &unsupported_op)) {
            return;
        }
    }

    jit_get_ic_layout(&ic_layout);

    /* Phase 1: identify branch targets */
    is_target = calloc(bc_len, 1);
    if (!is_target)
        return;

    for (pos = 0; pos < bc_len; ) {
        uint8_t op = bc[pos];
        int target;

        switch (op) {
        case OP_if_false:
        case OP_if_true:
        case OP_goto:
            target = pos + 1 + (int32_t)get_u32(bc + pos + 1);
            if (target >= 0 && target < bc_len)
                is_target[target] = 1;
            break;
        case OP_if_false8:
        case OP_if_true8:
        case OP_goto8:
            target = pos + 1 + (int8_t)bc[pos + 1];
            if (target >= 0 && target < bc_len)
                is_target[target] = 1;
            break;
        case OP_goto16:
            target = pos + 1 + (int16_t)get_u16(bc + pos + 1);
            if (target >= 0 && target < bc_len)
                is_target[target] = 1;
            break;
        case OP_catch:
            /* Catch handler target */
            target = (int)((pos + 1) + (int32_t)get_u32(bc + pos + 1));
            if (target >= 0 && target < bc_len)
                is_target[target] = 1;
            break;
        case OP_gosub:
            /* Finally block target */
            target = (int)((pos + 1) + (int32_t)get_u32(bc + pos + 1));
            if (target >= 0 && target < bc_len)
                is_target[target] = 1;
            /* Gosub return position (after this instruction) */
            if (pos + 5 < bc_len)
                is_target[pos + 5] = 1;
            break;
        case OP_with_get_var:
        case OP_with_put_var:
        case OP_with_delete_var:
        case OP_with_make_ref:
        case OP_with_get_ref:
        case OP_with_get_ref_undef:
            /* with_* branch target: pos + 5 + diff */
            {
                int32_t diff = (int32_t)get_u32(bc + pos + 5);
                target = pos + 5 + diff;
                if (target >= 0 && target < bc_len)
                    is_target[target] = 1;
            }
            break;
        }

        if (op == OP_initial_yield || op == OP_yield ||
            op == OP_yield_star || op == OP_async_yield_star ||
            op == OP_await || op == OP_return_async) {
            int resume_pos = pos + jit_opcode_size[op];
            if (resume_pos >= 0 && resume_pos < bc_len)
                is_target[resume_pos] = 1;
        }

        pos += jit_opcode_size[op];
    }

    /* Count dispatch table entries (catch targets + gosub return sites) */
    max_dispatch = 0;
    for (pos = 0; pos < bc_len; ) {
        uint8_t op = bc[pos];
        if (op == OP_catch)
            max_dispatch++;
        if (op == OP_gosub)
            max_dispatch++;  /* return site */
        if (op == OP_initial_yield || op == OP_yield ||
            op == OP_yield_star || op == OP_async_yield_star ||
            op == OP_await || op == OP_return_async)
            max_dispatch++;
        pos += jit_opcode_size[op];
    }

    /* Allocate dispatch table */
    if (max_dispatch > 0) {
        dispatch_table = js_mallocz(ctx, max_dispatch * sizeof(JitDispatchEntry));
        if (!dispatch_table)
            goto fail;
    }

    /* Count and allocate inline cache entries for property access */
    for (pos = 0; pos < bc_len; ) {
        uint8_t op = bc[pos];
        if (op == OP_get_field || op == OP_get_field2 || op == OP_put_field)
            ic_total++;
        pos += jit_opcode_size[op];
    }
    if (ic_total > 0) {
        ic_array = js_mallocz(ctx, ic_total * sizeof(PropIC));
        if (!ic_array)
            goto fail;
    }

    /* Fill dispatch table entries */
    for (pos = 0; pos < bc_len; ) {
        uint8_t op = bc[pos];
        if (op == OP_catch) {
            int target = (int)((pos + 1) + (int32_t)get_u32(bc + pos + 1));
            dispatch_table[n_dispatch].bc_pos = target;
            dispatch_table[n_dispatch].native_addr = NULL;
            n_dispatch++;
        }
        if (op == OP_gosub) {
            dispatch_table[n_dispatch].bc_pos = pos + 5;
            dispatch_table[n_dispatch].native_addr = NULL;
            n_dispatch++;
        }
        if (op == OP_initial_yield || op == OP_yield ||
            op == OP_yield_star || op == OP_async_yield_star ||
            op == OP_await || op == OP_return_async) {
            int resume_pos = pos + jit_opcode_size[op];
            dispatch_table[n_dispatch].bc_pos = resume_pos;
            dispatch_table[n_dispatch].native_addr = NULL;
            n_dispatch++;
        }
        pos += jit_opcode_size[op];
    }

    /* Allocate label array */
    labels = calloc(bc_len, sizeof(struct sljit_label *));
    if (!labels)
        goto fail;

    /* Allocate deferred jump list (max = number of branch instructions) */
    max_deferred = bc_len; /* overestimate */
    deferred = malloc(max_deferred * sizeof(JitJumpPatch));
    if (!deferred)
        goto fail;

    /* Allocate exception jump list */
    max_exc_jumps = bc_len;
    exc_jumps = malloc(max_exc_jumps * sizeof(struct sljit_jump *));
    if (!exc_jumps)
        goto fail;

    /* Phase 2: create sljit compiler and emit code */
    C = sljit_create_compiler(NULL);
    if (!C)
        goto fail;

#if (defined SLJIT_VERBOSE && SLJIT_VERBOSE)
    if (getenv("QJS_JIT_VERBOSE"))
        sljit_compiler_verbose(C, stdout);
#endif

    /* Function entry: int jit_func(JSContext *ctx, JitAux *aux)
     * S0 = ctx, S1 = aux (set by emit_enter from args)
     * We use 5 scratch regs (R0-R4) and 5 saved regs (S0-S4) */
    sljit_emit_enter(C, 0,
                     SLJIT_ARGS2(W, P, P),
                     5 /* scratches */, 5 /* saveds */,
                     0 /* local_size */);

    /* Load sp, var_buf, arg_buf from aux struct */
    sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
    sljit_emit_op1(C, SLJIT_MOV, REG_VBUF, 0,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, var_buf));
    sljit_emit_op1(C, SLJIT_MOV, REG_ABUF, 0,
                   SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, arg_buf));

    /* Generator resume: if resume_native_addr != NULL, jump to it */
    {
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0,
                       SLJIT_MEM1(REG_AUX),
                       (sljit_sw)offsetof(JitAux, resume_native_addr));
        struct sljit_jump *not_resume = sljit_emit_cmp(C, SLJIT_EQUAL,
                                                        SLJIT_R0, 0,
                                                        SLJIT_IMM, 0);
        sljit_emit_ijump(C, SLJIT_JUMP, SLJIT_R0, 0);
        sljit_set_label(not_resume, sljit_emit_label(C));
    }

    /* Phase 3: emit code for each opcode */
    for (pos = 0; pos < bc_len; ) {
        uint8_t op = bc[pos];
        int opsize = jit_opcode_size[op];

        /* Emit label at branch targets */
        if (is_target[pos])
            labels[pos] = sljit_emit_label(C);

        switch (op) {

        /* ---- Push constants ---- */

        case OP_push_i32:
            emit_push_const_jsv(C, JS_MKVAL(JS_TAG_INT, (int32_t)get_u32(bc + pos + 1)));
            break;

        case OP_push_minus1:
        case OP_push_0:
        case OP_push_1:
        case OP_push_2:
        case OP_push_3:
        case OP_push_4:
        case OP_push_5:
        case OP_push_6:
        case OP_push_7:
            emit_push_const_jsv(C, JS_MKVAL(JS_TAG_INT, op - OP_push_0));
            break;

        case OP_push_i8:
            emit_push_const_jsv(C, JS_MKVAL(JS_TAG_INT, (int8_t)bc[pos + 1]));
            break;

        case OP_push_i16:
            emit_push_const_jsv(C, JS_MKVAL(JS_TAG_INT, (int16_t)get_u16(bc + pos + 1)));
            break;

        case OP_undefined:
            emit_push_const_jsv(C, JS_UNDEFINED);
            break;

        case OP_null:
            emit_push_const_jsv(C, JS_NULL);
            break;

        case OP_push_false:
            emit_push_const_jsv(C, JS_FALSE);
            break;

        case OP_push_true:
            emit_push_const_jsv(C, JS_TRUE);
            break;

        /* ---- Stack manipulation ---- */

        case OP_drop:
            EMIT_DROP(C);
            break;

        case OP_dup:
            EMIT_DUP(C);
            break;

        case OP_swap:
            EMIT_SWAP(C);
            break;

        case OP_nip:
            EMIT_NIP(C);
            break;

        /* ---- Local variable access (3-byte: get_u16 index) ---- */

        case OP_get_loc:
            EMIT_GET_VAR(C, REG_VBUF, (sljit_sw)get_u16(bc + pos + 1));
            break;
        case OP_put_loc:
            EMIT_PUT_VAR(C, REG_VBUF, (sljit_sw)get_u16(bc + pos + 1));
            break;
        case OP_set_loc:
            emit_set_var(C, REG_VBUF, (sljit_sw)get_u16(bc + pos + 1));
            break;

        /* ---- Local variable access (2-byte: loc8 index) ---- */

        case OP_get_loc8:
            EMIT_GET_VAR(C, REG_VBUF, (sljit_sw)bc[pos + 1]);
            break;
        case OP_put_loc8:
            EMIT_PUT_VAR(C, REG_VBUF, (sljit_sw)bc[pos + 1]);
            break;
        case OP_set_loc8:
            emit_set_var(C, REG_VBUF, (sljit_sw)bc[pos + 1]);
            break;

        /* ---- Local variable access (1-byte short forms) ---- */

        case OP_get_loc0: EMIT_GET_VAR(C, REG_VBUF, 0); break;
        case OP_get_loc1: EMIT_GET_VAR(C, REG_VBUF, 1); break;
        case OP_get_loc2: EMIT_GET_VAR(C, REG_VBUF, 2); break;
        case OP_get_loc3: EMIT_GET_VAR(C, REG_VBUF, 3); break;

        case OP_put_loc0: EMIT_PUT_VAR(C, REG_VBUF, 0); break;
        case OP_put_loc1: EMIT_PUT_VAR(C, REG_VBUF, 1); break;
        case OP_put_loc2: EMIT_PUT_VAR(C, REG_VBUF, 2); break;
        case OP_put_loc3: EMIT_PUT_VAR(C, REG_VBUF, 3); break;

        case OP_set_loc0: emit_set_var(C, REG_VBUF, 0); break;
        case OP_set_loc1: emit_set_var(C, REG_VBUF, 1); break;
        case OP_set_loc2: emit_set_var(C, REG_VBUF, 2); break;
        case OP_set_loc3: emit_set_var(C, REG_VBUF, 3); break;

        /* ---- Multi-load shortcut ---- */

        case OP_get_loc0_loc1:
            EMIT_GET_VAR(C, REG_VBUF, 0);
            EMIT_GET_VAR(C, REG_VBUF, 1);
            break;

        /* ---- Argument access (3-byte: get_u16 index) ---- */

        case OP_get_arg:
            EMIT_GET_VAR(C, REG_ABUF, (sljit_sw)get_u16(bc + pos + 1));
            break;
        case OP_put_arg:
            EMIT_PUT_VAR(C, REG_ABUF, (sljit_sw)get_u16(bc + pos + 1));
            break;
        case OP_set_arg:
            emit_set_var(C, REG_ABUF, (sljit_sw)get_u16(bc + pos + 1));
            break;

        /* ---- Argument access (1-byte short forms) ---- */

        case OP_get_arg0: EMIT_GET_VAR(C, REG_ABUF, 0); break;
        case OP_get_arg1: EMIT_GET_VAR(C, REG_ABUF, 1); break;
        case OP_get_arg2: EMIT_GET_VAR(C, REG_ABUF, 2); break;
        case OP_get_arg3: EMIT_GET_VAR(C, REG_ABUF, 3); break;

        case OP_put_arg0: EMIT_PUT_VAR(C, REG_ABUF, 0); break;
        case OP_put_arg1: EMIT_PUT_VAR(C, REG_ABUF, 1); break;
        case OP_put_arg2: EMIT_PUT_VAR(C, REG_ABUF, 2); break;
        case OP_put_arg3: EMIT_PUT_VAR(C, REG_ABUF, 3); break;

        case OP_set_arg0: emit_set_var(C, REG_ABUF, 0); break;
        case OP_set_arg1: emit_set_var(C, REG_ABUF, 1); break;
        case OP_set_arg2: emit_set_var(C, REG_ABUF, 2); break;
        case OP_set_arg3: emit_set_var(C, REG_ABUF, 3); break;

        /* ---- Arithmetic (via icall to quickjs.c helpers) ---- */

        case OP_add:
            EMIT_ADD(C, exc_jumps, &n_exc_jumps);
            break;
        case OP_sub:
            EMIT_SUB(C, exc_jumps, &n_exc_jumps);
            break;
        case OP_mul:
            EMIT_MUL(C, exc_jumps, &n_exc_jumps);
            break;

        /* ---- Branches (5-byte: label32) ---- */

        case OP_if_false:
        case OP_if_true: {
            int target = pos + 1 + (int32_t)get_u32(bc + pos + 1);
            struct sljit_jump *j = EMIT_BRANCH(C,
                                               op == OP_if_true ? 1 : 0);
            if (labels[target]) {
                sljit_set_label(j, labels[target]);
            } else {
                deferred[n_deferred].jump = j;
                deferred[n_deferred].target_pc = target;
                n_deferred++;
            }
            break;
        }

        case OP_goto: {
            int target = pos + 1 + (int32_t)get_u32(bc + pos + 1);
            struct sljit_jump *j = sljit_emit_jump(C, SLJIT_JUMP);
            if (labels[target]) {
                sljit_set_label(j, labels[target]);
            } else {
                deferred[n_deferred].jump = j;
                deferred[n_deferred].target_pc = target;
                n_deferred++;
            }
            break;
        }

        /* ---- Branches (2-byte: label8) ---- */

        case OP_if_false8:
        case OP_if_true8: {
            int target = pos + 1 + (int8_t)bc[pos + 1];
            struct sljit_jump *j = EMIT_BRANCH(C,
                                               op == OP_if_true8 ? 1 : 0);
            if (labels[target]) {
                sljit_set_label(j, labels[target]);
            } else {
                deferred[n_deferred].jump = j;
                deferred[n_deferred].target_pc = target;
                n_deferred++;
            }
            break;
        }

        case OP_goto8: {
            int target = pos + 1 + (int8_t)bc[pos + 1];
            struct sljit_jump *j = sljit_emit_jump(C, SLJIT_JUMP);
            if (labels[target]) {
                sljit_set_label(j, labels[target]);
            } else {
                deferred[n_deferred].jump = j;
                deferred[n_deferred].target_pc = target;
                n_deferred++;
            }
            break;
        }

        /* ---- Branches (3-byte: label16) ---- */

        case OP_goto16: {
            int target = pos + 1 + (int16_t)get_u16(bc + pos + 1);
            struct sljit_jump *j = sljit_emit_jump(C, SLJIT_JUMP);
            if (labels[target]) {
                sljit_set_label(j, labels[target]);
            } else {
                deferred[n_deferred].jump = j;
                deferred[n_deferred].target_pc = target;
                n_deferred++;
            }
            break;
        }

        /* ---- Return ---- */

        case OP_return:
            EMIT_RETURN(C);
            break;

        case OP_return_undef:
            EMIT_RETURN_UNDEF(C);
            break;

        /* ---- No-op ---- */

        case OP_nop:
            break;

        /* ---- Exception handling: OP_gosub (custom emitter) ---- */

        case OP_gosub: {
            int return_pos = pos + 5;
            int target = pos + 1 + (int32_t)get_u32(bc + pos + 1);
            /* Push js_int32(return_pos) onto JS stack */
            emit_push_const_jsv(C, JS_MKVAL(JS_TAG_INT, return_pos));
            /* Jump to target (finally block) */
            {
                struct sljit_jump *j = sljit_emit_jump(C, SLJIT_JUMP);
                if (labels[target]) {
                    sljit_set_label(j, labels[target]);
                } else {
                    deferred[n_deferred].jump = j;
                    deferred[n_deferred].target_pc = target;
                    n_deferred++;
                }
            }
            break;
        }

        /* ---- Exception handling: OP_ret (custom emitter) ---- */

        case OP_ret: {
            /* Save sp to aux */
            sljit_emit_op1(C, SLJIT_MOV,
                           SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                           REG_SP, 0);
            /* Call qjs_jit_ret(ctx, aux) → returns native address */
            sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
            sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
            sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(P, P, P),
                             SLJIT_IMM, SLJIT_FUNC_ADDR(qjs_jit_ret));
            /* Reload sp from aux */
            sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                           SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
            /* Check: NULL = error → exception */
            {
                struct sljit_jump *j = sljit_emit_cmp(C, SLJIT_EQUAL,
                                                       SLJIT_RETURN_REG, 0,
                                                       SLJIT_IMM, 0);
                exc_jumps[n_exc_jumps++] = j;
            }
            /* Indirect jump to native return address */
            sljit_emit_ijump(C, SLJIT_JUMP, SLJIT_RETURN_REG, 0);
            break;
        }

        /* ---- with_* opcodes (custom emitter with 3-way return) ---- */

        case OP_with_get_var:
        case OP_with_put_var:
        case OP_with_delete_var:
        case OP_with_make_ref:
        case OP_with_get_ref:
        case OP_with_get_ref_undef: {
            int32_t diff = (int32_t)get_u32(bc + pos + 5);
            int target = pos + 5 + diff;

            /* Save sp to aux */
            sljit_emit_op1(C, SLJIT_MOV,
                           SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                           REG_SP, 0);
            /* Call jit_op_with(ctx, aux, pc) */
            sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
            sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
            sljit_emit_op1(C, SLJIT_MOV, SLJIT_R2, 0, SLJIT_IMM,
                           (sljit_sw)(bc + pos));
            sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS3(W, P, P, P),
                             SLJIT_IMM, SLJIT_FUNC_ADDR(jit_op_with));
            /* Reload sp, var_buf, arg_buf from aux */
            sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                           SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
            sljit_emit_op1(C, SLJIT_MOV, REG_VBUF, 0,
                           SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, var_buf));
            sljit_emit_op1(C, SLJIT_MOV, REG_ABUF, 0,
                           SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, arg_buf));
            /* Sign-extend 32-bit int return to word size (required on Win64
               where mov eax,-1 zero-extends to 0x00000000FFFFFFFF) */
            sljit_emit_op1(C, SLJIT_MOV_S32, SLJIT_RETURN_REG, 0,
                           SLJIT_RETURN_REG, 0);
            {
                struct sljit_jump *j = sljit_emit_cmp(C, SLJIT_SIG_LESS,
                                                       SLJIT_RETURN_REG, 0,
                                                       SLJIT_IMM, 0);
                exc_jumps[n_exc_jumps++] = j;
            }
            {
                struct sljit_jump *j = sljit_emit_cmp(C, SLJIT_EQUAL,
                                                       SLJIT_RETURN_REG, 0,
                                                       SLJIT_IMM, 1);
                if (labels[target]) {
                    sljit_set_label(j, labels[target]);
                } else {
                    deferred[n_deferred].jump = j;
                    deferred[n_deferred].target_pc = target;
                    n_deferred++;
                }
            }
            /* Return 0 = fall-through (not found), continue */
            break;
        }

        /* ---- Push constants (not natively handled) ---- */
        case OP_push_const: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_push_const, bc + pos); break;
        case OP_fclosure: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_fclosure, bc + pos); break;
        case OP_push_atom_value: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_push_atom_value, bc + pos); break;
        case OP_private_symbol: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_private_symbol, bc + pos); break;
        case OP_push_this: EMIT_PUSH_THIS(C, exc_jumps, &n_exc_jumps); break;
        case OP_object: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_object); break;
        case OP_special_object: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_special_object, bc + pos); break;
        case OP_rest: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_rest, bc + pos); break;
        case OP_push_const8: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_push_const8, bc + pos); break;
        case OP_fclosure8: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_fclosure8, bc + pos); break;
        case OP_push_empty_string: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_push_literal, OP_push_empty_string); break;
        case OP_push_bigint_i32: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_push_bigint_i32, bc + pos); break;
        /* ---- Stack manipulation (not natively handled) ---- */
        case OP_nip1: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_nip1); break;
        case OP_dup1: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_dup1); break;
        case OP_dup2: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_dup2); break;
        case OP_dup3: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_dup3); break;
        case OP_insert2: EMIT_INSERT2(C); break;
        case OP_insert3: EMIT_INSERT3(C); break;
        case OP_insert4: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_insert4); break;
        case OP_perm3: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_perm3); break;
        case OP_perm4: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_perm4); break;
        case OP_perm5: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_perm5); break;
        case OP_swap2: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_swap2); break;
        case OP_rot3l: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_rot3l); break;
        case OP_rot3r: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_rot3r); break;
        case OP_rot4l: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_rot4l); break;
        case OP_rot5l: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_rot5l); break;
        /* ---- Calls ---- */
        case OP_call: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_call, bc + pos); break;
        case OP_call_constructor: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_call_constructor, bc + pos); break;
        case OP_call_method: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_call_method, bc + pos); break;
        case OP_tail_call: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_call, bc + pos); EMIT_RETURN(C); break;
        case OP_tail_call_method: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_call_method, bc + pos); EMIT_RETURN(C); break;
        case OP_array_from: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_array_from, bc + pos); break;
        case OP_apply: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_apply, bc + pos); break;
        case OP_call0: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_call_n, 0); break;
        case OP_call1: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_call_n, 1); break;
        case OP_call2: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_call_n, 2); break;
        case OP_call3: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_call_n, 3); break;
        /* ---- Constructor/brand ---- */
        case OP_check_ctor_return: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_check_ctor_return); break;
        case OP_check_ctor: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_check_ctor); break;
        case OP_init_ctor: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_init_ctor); break;
        case OP_check_brand: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_check_brand); break;
        case OP_add_brand: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_add_brand); break;
        /* ---- Throw/eval/misc ---- */
        case OP_throw: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_throw); break;
        case OP_throw_error: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_throw_error, bc + pos); break;
        case OP_eval: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_eval, bc + pos); break;
        case OP_apply_eval: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_apply_eval, bc + pos); break;
        case OP_regexp: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_regexp); break;
        case OP_get_super: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_get_super); break;
        case OP_import: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_import); break;
        /* ---- Global variables ---- */
        case OP_get_var: case OP_get_var_undef: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_get_var, bc + pos); break;
        case OP_put_var: case OP_put_var_init: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_put_var, bc + pos); break;
        case OP_check_define_var: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_check_define_var, bc + pos); break;
        case OP_define_var: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_define_var, bc + pos); break;
        case OP_define_func: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_define_func, bc + pos); break;
        /* ---- Ref value ---- */
        case OP_get_ref_value: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_get_ref_value); break;
        case OP_put_ref_value: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_put_ref_value); break;
        /* ---- Property access ---- */
        case OP_get_field: { PropIC *ic = ic_array ? &ic_array[ic_idx++] : NULL; if (ic) emit_get_field_ic_fast(C, bc + pos, ic, &ic_layout, exc_jumps, &n_exc_jumps); else exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_get_field, bc + pos); break; }
        case OP_get_field2: { PropIC *ic = ic_array ? &ic_array[ic_idx++] : NULL; if (ic) emit_get_field2_ic_fast(C, bc + pos, ic, &ic_layout, exc_jumps, &n_exc_jumps); else exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_get_field2, bc + pos); break; }
        case OP_put_field: { PropIC *ic = ic_array ? &ic_array[ic_idx++] : NULL; if (ic) emit_put_field_ic_fast(C, bc + pos, ic, &ic_layout, exc_jumps, &n_exc_jumps); else exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_put_field, bc + pos); break; }
        case OP_get_private_field: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_get_private_field); break;
        case OP_put_private_field: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_put_private_field); break;
        case OP_define_private_field: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_define_private_field); break;
        case OP_get_array_el: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_get_array_el); break;
        case OP_get_array_el2: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_get_array_el2); break;
        case OP_put_array_el: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_put_array_el); break;
        case OP_get_super_value: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_get_super_value); break;
        case OP_put_super_value: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_put_super_value); break;
        case OP_get_length: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_get_length); break;
        /* ---- Define ---- */
        case OP_define_field: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_define_field, bc + pos); break;
        case OP_set_name: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_set_name, bc + pos); break;
        case OP_set_name_computed: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_set_name_computed); break;
        case OP_set_proto: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_set_proto); break;
        case OP_set_home_object: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_set_home_object); break;
        case OP_define_array_el: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_define_array_el); break;
        case OP_append: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_append); break;
        case OP_copy_data_properties: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_copy_data_properties, bc + pos); break;
        case OP_define_method: case OP_define_method_computed: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_define_method, bc + pos); break;
        case OP_define_class: case OP_define_class_computed: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_define_class, bc + pos); break;
        /* ---- Var ref (3-byte) ---- */
        case OP_get_var_ref: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_get_var_ref, (sljit_sw)get_u16(bc + pos + 1)); break;
        case OP_put_var_ref: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_put_var_ref, (sljit_sw)get_u16(bc + pos + 1)); break;
        case OP_set_var_ref: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_set_var_ref, (sljit_sw)get_u16(bc + pos + 1)); break;
        case OP_get_var_ref_check: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_get_var_ref_check, bc + pos); break;
        case OP_put_var_ref_check: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_put_var_ref_check, bc + pos); break;
        case OP_put_var_ref_check_init: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_put_var_ref_check_init, bc + pos); break;
        /* ---- Var ref short forms ---- */
        case OP_get_var_ref0: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_get_var_ref, 0); break;
        case OP_get_var_ref1: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_get_var_ref, 1); break;
        case OP_get_var_ref2: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_get_var_ref, 2); break;
        case OP_get_var_ref3: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_get_var_ref, 3); break;
        case OP_put_var_ref0: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_put_var_ref, 0); break;
        case OP_put_var_ref1: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_put_var_ref, 1); break;
        case OP_put_var_ref2: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_put_var_ref, 2); break;
        case OP_put_var_ref3: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_put_var_ref, 3); break;
        case OP_set_var_ref0: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_set_var_ref, 0); break;
        case OP_set_var_ref1: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_set_var_ref, 1); break;
        case OP_set_var_ref2: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_set_var_ref, 2); break;
        case OP_set_var_ref3: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_set_var_ref, 3); break;
        /* ---- Checked loc ---- */
        case OP_set_loc_uninitialized: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_set_loc_uninitialized, bc + pos); break;
        case OP_get_loc_check: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_get_loc_check, bc + pos); break;
        case OP_put_loc_check: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_put_loc_check, bc + pos); break;
        case OP_put_loc_check_init: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_put_loc_check_init, bc + pos); break;
        case OP_close_loc: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_close_loc, bc + pos); break;
        /* ---- Exception ---- */
        case OP_catch: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_catch, bc + pos); break;
        case OP_nip_catch: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_nip_catch); break;
        /* ---- Conversion ---- */
        case OP_to_object: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_to_object); break;
        case OP_to_propkey: EMIT_TO_PROPKEY(C, exc_jumps, &n_exc_jumps); break;
        case OP_to_propkey2: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_to_propkey2); break;
        /* ---- Make refs ---- */
        case OP_make_loc_ref: case OP_make_arg_ref: case OP_make_var_ref_ref:
            exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_make_ref, bc + pos); break;
        case OP_make_var_ref: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_make_var_ref, bc + pos); break;
        /* ---- Iterators ---- */
        case OP_for_in_start: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_for_in_start); break;
        case OP_for_of_start: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_for_of_start); break;
        case OP_for_in_next: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_for_in_next); break;
        case OP_for_of_next: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_for_of_next, bc + pos); break;
        case OP_iterator_check_object: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_iterator_check_object); break;
        case OP_iterator_get_value_done: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_iterator_get_value_done); break;
        case OP_iterator_close: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_iterator_close); break;
        case OP_iterator_next: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_iterator_next); break;
        case OP_iterator_call: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_iterator_call, bc + pos); break;
        /* ---- Unary arithmetic ---- */
        case OP_neg: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_neg); break;
        case OP_plus: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_plus); break;
        case OP_dec: EMIT_INC_DEC(C, 1, exc_jumps, &n_exc_jumps); break;
        case OP_inc: EMIT_INC_DEC(C, 0, exc_jumps, &n_exc_jumps); break;
        case OP_post_dec: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_post_dec); break;
        case OP_post_inc: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_post_inc); break;
        case OP_dec_loc: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_dec_loc, bc + pos); break;
        case OP_inc_loc: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_inc_loc, bc + pos); break;
        case OP_add_loc: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_add_loc, bc + pos); break;
        case OP_not: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_not); break;
        case OP_lnot: EMIT_LNOT(C, exc_jumps, &n_exc_jumps); break;
        case OP_typeof: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_typeof); break;
        case OP_delete: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_delete); break;
        case OP_delete_var: exc_jumps[n_exc_jumps++] = emit_op_call_pc(C, (void *)jit_op_delete_var, bc + pos); break;
        /* ---- Binary arithmetic (add/sub/mul already natively handled) ---- */
        case OP_div: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_div); break;
        case OP_mod: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_mod); break;
        case OP_pow: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_pow); break;
        case OP_shl: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_shl); break;
        case OP_sar: exc_jumps[n_exc_jumps++] = emit_op_call_int(C, (void *)jit_op_binary_logic, OP_sar); break;
        case OP_shr: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_shr); break;
        case OP_and: EMIT_BINARY_LOGIC(C, OP_and, exc_jumps, &n_exc_jumps); break;
        case OP_xor: EMIT_BINARY_LOGIC(C, OP_xor, exc_jumps, &n_exc_jumps); break;
        case OP_or: EMIT_BINARY_LOGIC(C, OP_or, exc_jumps, &n_exc_jumps); break;
        /* ---- Comparison ---- */
        case OP_lt: EMIT_RELATIONAL(C, OP_lt, exc_jumps, &n_exc_jumps); break;
        case OP_lte: EMIT_RELATIONAL(C, OP_lte, exc_jumps, &n_exc_jumps); break;
        case OP_gt: EMIT_RELATIONAL(C, OP_gt, exc_jumps, &n_exc_jumps); break;
        case OP_gte: EMIT_RELATIONAL(C, OP_gte, exc_jumps, &n_exc_jumps); break;
        case OP_instanceof: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_instanceof); break;
        case OP_in: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_in); break;
        case OP_eq: EMIT_EQ(C, OP_eq, exc_jumps, &n_exc_jumps); break;
        case OP_neq: EMIT_EQ(C, OP_neq, exc_jumps, &n_exc_jumps); break;
        case OP_strict_eq: EMIT_EQ(C, OP_strict_eq, exc_jumps, &n_exc_jumps); break;
        case OP_strict_neq: EMIT_EQ(C, OP_strict_neq, exc_jumps, &n_exc_jumps); break;
        case OP_is_undefined_or_null: EMIT_IS_UNDEF_OR_NULL(C, exc_jumps, &n_exc_jumps); break;
        case OP_private_in: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_private_in); break;
        /* ---- Type checks ---- */
        case OP_is_undefined: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_is_undefined); break;
        case OP_is_null: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_is_null); break;
        case OP_typeof_is_undefined: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_typeof_is_undefined); break;
        case OP_typeof_is_function: exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_typeof_is_function); break;
        /* ---- Generator/async opcodes ---- */
        case OP_await:
            emit_generator_suspend(C, 0, bc + pos + opsize);
            break;
        case OP_yield:
            emit_generator_suspend(C, 1, bc + pos + opsize);
            break;
        case OP_yield_star:
        case OP_async_yield_star:
            emit_generator_suspend(C, 2, bc + pos + opsize);
            break;
        case OP_initial_yield:
        case OP_return_async:
            emit_generator_suspend(C, 3, bc + pos + opsize);
            break;
        case OP_for_await_of_start:
            exc_jumps[n_exc_jumps++] = emit_op_call_2(C, (void *)jit_op_for_await_of_start);
            break;
        case OP_invalid:
        default:
            abort(); break;
        }

        pos += opsize;
    }

    /* Patch deferred forward jumps */
    {
        int i;
        for (i = 0; i < n_deferred; i++) {
            struct sljit_label *lbl = labels[deferred[i].target_pc];
            if (!lbl)
                goto fail_compiler; /* should not happen */
            sljit_set_label(deferred[i].jump, lbl);
        }
    }

    /* Emit exception handler: unwind stack, dispatch to catch or propagate */
    {
        struct sljit_label *exc_label = sljit_emit_label(C);
        int i;
        for (i = 0; i < n_exc_jumps; i++) {
            sljit_set_label(exc_jumps[i], exc_label);
        }
        /* Store current sp to aux */
        sljit_emit_op1(C, SLJIT_MOV,
                       SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp),
                       REG_SP, 0);
        /* Call jit_unwind_exception(ctx, aux) → native addr or NULL */
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R0, 0, REG_CTX, 0);
        sljit_emit_op1(C, SLJIT_MOV, SLJIT_R1, 0, REG_AUX, 0);
        sljit_emit_icall(C, SLJIT_CALL, SLJIT_ARGS2(P, P, P),
                         SLJIT_IMM, SLJIT_FUNC_ADDR(jit_unwind_exception));
        /* Check: NULL = no handler found, propagate to caller */
        {
            struct sljit_jump *no_handler = sljit_emit_cmp(C, SLJIT_EQUAL,
                                                            SLJIT_RETURN_REG, 0,
                                                            SLJIT_IMM, 0);
            /* Handler found — save native addr, reload registers, jump */
            sljit_emit_op1(C, SLJIT_MOV, SLJIT_R4, 0,
                           SLJIT_RETURN_REG, 0);
            sljit_emit_op1(C, SLJIT_MOV, REG_SP, 0,
                           SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, sp));
            sljit_emit_op1(C, SLJIT_MOV, REG_VBUF, 0,
                           SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, var_buf));
            sljit_emit_op1(C, SLJIT_MOV, REG_ABUF, 0,
                           SLJIT_MEM1(REG_AUX), (sljit_sw)offsetof(JitAux, arg_buf));
            /* Indirect jump to catch handler native code */
            sljit_emit_ijump(C, SLJIT_JUMP, SLJIT_R4, 0);

            /* No handler — propagate exception to caller */
            sljit_set_label(no_handler, sljit_emit_label(C));
            sljit_emit_return(C, SLJIT_MOV, SLJIT_IMM, 1);
        }
    }

    /* Generate executable code */
    code = sljit_generate_code(C, 0, NULL);
    if (!code)
        goto fail_compiler;

    *out_jitcode = (JitFunc)code;
    *out_jit_code_ptr = code;

    /* Fill in dispatch table native addresses from labels */
    {
        int i;
        for (i = 0; i < n_dispatch; i++) {
            int bc_pos_i = dispatch_table[i].bc_pos;
            if (bc_pos_i < 0 || bc_pos_i >= bc_len) {
                dispatch_table[i].native_addr = NULL;
                continue;
            }
            struct sljit_label *lbl = labels[bc_pos_i];
            if (lbl) {
                dispatch_table[i].native_addr =
                    (void *)sljit_get_label_addr(lbl);
            } else {
                dispatch_table[i].native_addr = NULL;
            }
        }
        *out_dispatch_table = dispatch_table;
        *out_dispatch_count = n_dispatch;
        dispatch_table = NULL; /* prevent free in cleanup */
    }

    *out_ic_cache = ic_array;
    *out_ic_count = ic_total;
    ic_array = NULL; /* prevent free in cleanup */

    sljit_free_compiler(C);
    free(is_target);
    free(labels);
    free(deferred);
    free(exc_jumps);
    return;

fail_compiler:
    sljit_free_compiler(C);
fail:
    free(is_target);
    free(labels);
    free(deferred);
    free(exc_jumps);
    free(dispatch_table);
    free(ic_array);
    *out_jitcode = NULL;
    *out_jit_code_ptr = NULL;
    *out_dispatch_table = NULL;
    *out_dispatch_count = 0;
    *out_ic_cache = NULL;
    *out_ic_count = 0;
}

void js_sljit_free(void *jit_code_ptr)
{
    if (jit_code_ptr)
        sljit_free_code(jit_code_ptr, NULL);
}
