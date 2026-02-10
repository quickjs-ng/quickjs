/*
 * QuickJS JIT subsystem — shared header
 *
 * Defines the JitAux struct and declares functions shared between
 * quickjs.c (interpreter / arithmetic helpers) and quickjs_sljit.c
 * (code generation).
 *
 * This header deliberately avoids including any sljit or internal
 * quickjs.c headers so it can be consumed by both translation units
 * without pulling in implementation details.
 */

#ifndef QUICKJS_JIT_H
#define QUICKJS_JIT_H

#include "quickjs.h"

#ifdef CONFIG_JIT

/* ----- forward declarations (opaque to this header) ----- */

struct JSVarRef;
struct JSStackFrame;

/* ---- Monomorphic inline cache for property access ---- */
typedef struct PropIC {
    void *cached_shape;       /* JSShape* — ref-counted (pinned) to prevent ABA */
    uint32_t cached_offset;   /* 0-based index into JSObject.prop[] */
} PropIC;

/* ---- Struct layout info for inline IC code generation ---- */
typedef struct JitICLayout {
    int obj_shape_off;   /* offsetof(JSObject, shape) */
    int obj_prop_off;    /* offsetof(JSObject, prop) */
    int prop_size;       /* sizeof(JSProperty) — stride for prop[] array */
} JitICLayout;

void jit_get_ic_layout(JitICLayout *out);

/* ----- JitDispatchEntry: maps bytecode position → native address ----- */

/*
 * Used by the JIT exception handler (to dispatch to catch handlers)
 * and by OP_ret (to return from finally blocks to the correct gosub
 * call site).  Built during JIT compilation, native addresses filled
 * in after sljit_generate_code().
 */
typedef struct JitDispatchEntry {
    int   bc_pos;        /* absolute bytecode position                  */
    void *native_addr;   /* native code address (from sljit label)      */
} JitDispatchEntry;

/* ----- JitAux: per-call context passed from interpreter to JIT ----- */

/*
 * JitAux bundles every piece of interpreter state that compiled JIT
 * code needs to read or write.  The field order is fixed — generated
 * code accesses members via hard-coded offsetof values.
 */
typedef struct JitAux {
    JSValue            *stack_buf;   /* operand stack base              */
    JSValue            *var_buf;     /* local variable slots            */
    JSValue            *arg_buf;     /* function argument slots         */
    JSValue            *sp;          /* current stack pointer           */
    struct JSVarRef   **var_refs;    /* closure variable references     */
    struct JSStackFrame *sf;         /* current stack frame             */
    void               *p;          /* JSObject* — opaque to JIT code  */
    JSContext          *caller_ctx;  /* calling context                 */
    JSValue             ret_val;     /* JIT stores its return value here */
    void               *b;          /* JSFunctionBytecode* — for cpool */
    JSValue             this_obj;    /* 'this' value for the call       */
    JSValue             new_target;  /* new.target value                */
    JSValue             func_obj;    /* the function being called       */
    int                 argc;        /* argument count                  */
    const JSValue      *argv;        /* argument values                 */
    JitDispatchEntry   *dispatch_table;  /* catch/return dispatch table */
    int                 dispatch_count;  /* number of dispatch entries  */
    PropIC             *ic_cache;       /* inline cache array for property access */
    int                 ic_count;       /* number of IC entries */
    void               *resume_native_addr;  /* generator resume: native addr to jump to (NULL for initial call) */
    const uint8_t      *resume_bc_pc;  /* generator suspend: bytecode pc to resume at */
} JitAux;

/* ----- JitFunc: signature of every compiled JIT entry point ----- */

/*
 * Returns 0 on normal return (result in aux->ret_val).
 * Returns non-zero on exception.
 */
typedef int (*JitFunc)(JSContext *ctx, JitAux *aux);

/* ----- code generation (quickjs_sljit.c) ----- */

/*
 * Compile a bytecode function into native code via sljit.
 *
 * On success *out_jitcode receives the callable function pointer and
 * *out_jit_code_ptr receives the opaque allocation that must later be
 * freed with js_sljit_free().
 *
 * *out_dispatch_table / *out_dispatch_count receive the dispatch table
 * mapping bytecode positions to native addresses (for catch handlers
 * and gosub return sites).  Caller must free *out_dispatch_table.
 */
void js_sljit_compile(JSContext *ctx,
                      uint8_t *byte_code_buf, int byte_code_len,
                      int arg_count, int var_count, int stack_size,
                      JitFunc *out_jitcode, void **out_jit_code_ptr,
                      JitDispatchEntry **out_dispatch_table,
                      int *out_dispatch_count,
                      PropIC **out_ic_cache,
                      int *out_ic_count);

/* Free native code previously allocated by js_sljit_compile(). */
void js_sljit_free(void *jit_code_ptr);

/* ----- arithmetic helpers (quickjs.c) ----- */

/*
 * These helpers perform the OP_add / OP_sub / OP_mul operations,
 * including the slow paths that handle non-int32 operands.  They are
 * defined in quickjs.c because they need access to internal functions
 * (js_add_slow, js_binary_arith_slow, etc.).
 *
 * Each function operates on sp[-2] and sp[-1], replaces sp[-2] with
 * the result.  The caller (sljit code) handles the sp decrement.
 *
 * Returns 0 on success, non-zero on exception.
 */
int qjs_jit_add(JSContext *ctx, JSValue *sp);
int qjs_jit_sub(JSContext *ctx, JSValue *sp);
int qjs_jit_mul(JSContext *ctx, JSValue *sp);

/*
 * JIT exception handler: unwind the JS stack looking for a catch
 * offset, then look up the corresponding native code address in the
 * dispatch table.
 *
 * Returns the native address to jump to (catch handler), or NULL
 * if no handler was found (exception should propagate to caller).
 *
 * On success: aux->sp is updated, exception value is on the stack.
 */
void *jit_unwind_exception(JSContext *ctx, JitAux *aux);

/*
 * OP_ret helper: pop return address from JS stack, look up the
 * native code address in the dispatch table.
 *
 * Returns the native address to jump to, or NULL on error.
 */
void *qjs_jit_ret(JSContext *ctx, JitAux *aux);

/* ----- per-opcode JIT helpers (quickjs.c) ----- */
int jit_op_push_i32(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_push_i8(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_push_i16(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_push_small_int(JSContext *ctx, JitAux *aux, int opcode);
int jit_op_push_const(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_push_const8(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_push_bigint_i32(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_push_atom_value(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_push_literal(JSContext *ctx, JitAux *aux, int opcode);
int jit_op_object(JSContext *ctx, JitAux *aux);
int jit_op_push_this(JSContext *ctx, JitAux *aux);
int jit_op_special_object(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_rest(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_fclosure(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_fclosure8(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_drop(JSContext *ctx, JitAux *aux);
int jit_op_nip(JSContext *ctx, JitAux *aux);
int jit_op_dup(JSContext *ctx, JitAux *aux);
int jit_op_swap(JSContext *ctx, JitAux *aux);
int jit_op_get_loc(JSContext *ctx, JitAux *aux, int idx);
int jit_op_put_loc(JSContext *ctx, JitAux *aux, int idx);
int jit_op_set_loc(JSContext *ctx, JitAux *aux, int idx);
int jit_op_get_loc0_loc1(JSContext *ctx, JitAux *aux);
int jit_op_get_arg(JSContext *ctx, JitAux *aux, int idx);
int jit_op_put_arg(JSContext *ctx, JitAux *aux, int idx);
int jit_op_set_arg(JSContext *ctx, JitAux *aux, int idx);
int jit_op_get_var_ref(JSContext *ctx, JitAux *aux, int idx);
int jit_op_put_var_ref(JSContext *ctx, JitAux *aux, int idx);
int jit_op_set_var_ref(JSContext *ctx, JitAux *aux, int idx);
int jit_op_get_var_ref_check(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_put_var_ref_check(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_put_var_ref_check_init(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_set_loc_uninitialized(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_get_loc_check(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_put_loc_check(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_put_loc_check_init(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_close_loc(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_get_var(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_put_var(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_check_define_var(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_define_var(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_define_func(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_make_ref(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_make_var_ref(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_call_n(JSContext *ctx, JitAux *aux, int argc);
int jit_op_call(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_call_constructor(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_call_method(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_array_from(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_apply(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_check_ctor_return(JSContext *ctx, JitAux *aux);
int jit_op_check_ctor(JSContext *ctx, JitAux *aux);
int jit_op_init_ctor(JSContext *ctx, JitAux *aux);
int jit_op_check_brand(JSContext *ctx, JitAux *aux);
int jit_op_add_brand(JSContext *ctx, JitAux *aux);
int jit_op_throw(JSContext *ctx, JitAux *aux);
int jit_op_throw_error(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_eval(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_apply_eval(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_regexp(JSContext *ctx, JitAux *aux);
int jit_op_get_super(JSContext *ctx, JitAux *aux);
int jit_op_import(JSContext *ctx, JitAux *aux);
int jit_op_catch(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_nip_catch(JSContext *ctx, JitAux *aux);
int jit_op_for_in_start(JSContext *ctx, JitAux *aux);
int jit_op_for_in_next(JSContext *ctx, JitAux *aux);
int jit_op_for_of_start(JSContext *ctx, JitAux *aux);
int jit_op_for_of_next(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_iterator_get_value_done(JSContext *ctx, JitAux *aux);
int jit_op_iterator_check_object(JSContext *ctx, JitAux *aux);
int jit_op_iterator_close(JSContext *ctx, JitAux *aux);
int jit_op_iterator_next(JSContext *ctx, JitAux *aux);
int jit_op_iterator_call(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_lnot(JSContext *ctx, JitAux *aux);
int jit_op_get_field(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_get_field2(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_put_field(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_get_field_ic(JSContext *ctx, JitAux *aux, const uint8_t *pc, PropIC *ic);
int jit_op_get_field2_ic(JSContext *ctx, JitAux *aux, const uint8_t *pc, PropIC *ic);
int jit_op_put_field_ic(JSContext *ctx, JitAux *aux, const uint8_t *pc, PropIC *ic);
int jit_op_put_field_ic_hit(JSContext *ctx, JitAux *aux, PropIC *ic);
int jit_op_get_length(JSContext *ctx, JitAux *aux);
int jit_op_private_symbol(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_get_private_field(JSContext *ctx, JitAux *aux);
int jit_op_put_private_field(JSContext *ctx, JitAux *aux);
int jit_op_define_private_field(JSContext *ctx, JitAux *aux);
int jit_op_get_array_el(JSContext *ctx, JitAux *aux);
int jit_op_get_array_el2(JSContext *ctx, JitAux *aux);
int jit_op_get_ref_value(JSContext *ctx, JitAux *aux);
int jit_op_get_super_value(JSContext *ctx, JitAux *aux);
int jit_op_put_array_el(JSContext *ctx, JitAux *aux);
int jit_op_put_ref_value(JSContext *ctx, JitAux *aux);
int jit_op_put_super_value(JSContext *ctx, JitAux *aux);
int jit_op_define_field(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_set_name(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_set_name_computed(JSContext *ctx, JitAux *aux);
int jit_op_set_proto(JSContext *ctx, JitAux *aux);
int jit_op_set_home_object(JSContext *ctx, JitAux *aux);
int jit_op_define_array_el(JSContext *ctx, JitAux *aux);
int jit_op_append(JSContext *ctx, JitAux *aux);
int jit_op_copy_data_properties(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_define_method(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_define_class(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_add(JSContext *ctx, JitAux *aux);
int jit_op_sub(JSContext *ctx, JitAux *aux);
int jit_op_mul(JSContext *ctx, JitAux *aux);
int jit_op_div(JSContext *ctx, JitAux *aux);
int jit_op_mod(JSContext *ctx, JitAux *aux);
int jit_op_pow(JSContext *ctx, JitAux *aux);
int jit_op_add_loc(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_plus(JSContext *ctx, JitAux *aux);
int jit_op_neg(JSContext *ctx, JitAux *aux);
int jit_op_inc(JSContext *ctx, JitAux *aux);
int jit_op_dec(JSContext *ctx, JitAux *aux);
int jit_op_post_inc(JSContext *ctx, JitAux *aux);
int jit_op_post_dec(JSContext *ctx, JitAux *aux);
int jit_op_inc_loc(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_dec_loc(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_not(JSContext *ctx, JitAux *aux);
int jit_op_shl(JSContext *ctx, JitAux *aux);
int jit_op_shr(JSContext *ctx, JitAux *aux);
int jit_op_binary_logic(JSContext *ctx, JitAux *aux, int opcode);
int jit_op_relational(JSContext *ctx, JitAux *aux, int opcode);
int jit_op_eq(JSContext *ctx, JitAux *aux, int opcode);
int jit_op_strict_eq(JSContext *ctx, JitAux *aux, int opcode);
int jit_op_in(JSContext *ctx, JitAux *aux);
int jit_op_private_in(JSContext *ctx, JitAux *aux);
int jit_op_instanceof(JSContext *ctx, JitAux *aux);
int jit_op_typeof(JSContext *ctx, JitAux *aux);
int jit_op_delete(JSContext *ctx, JitAux *aux);
int jit_op_delete_var(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_to_object(JSContext *ctx, JitAux *aux);
int jit_op_to_propkey(JSContext *ctx, JitAux *aux);
int jit_op_to_propkey2(JSContext *ctx, JitAux *aux);
int jit_op_is_undefined_or_null(JSContext *ctx, JitAux *aux);
int jit_op_is_undefined(JSContext *ctx, JitAux *aux);
int jit_op_is_null(JSContext *ctx, JitAux *aux);
int jit_op_typeof_is_undefined(JSContext *ctx, JitAux *aux);
int jit_op_typeof_is_function(JSContext *ctx, JitAux *aux);
int jit_op_with(JSContext *ctx, JitAux *aux, const uint8_t *pc);
int jit_op_for_await_of_start(JSContext *ctx, JitAux *aux);

#endif /* CONFIG_JIT */

#endif /* QUICKJS_JIT_H */
