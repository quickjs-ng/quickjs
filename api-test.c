#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#include "cutils.h"

static JSRuntime *new_runtime(void)
{
    JSRuntime *rt = JS_NewRuntime();

    if (rt)
        JS_SetDumpFlags(rt, JS_ABORT_ON_LEAKS);
    return rt;
}

static JSValue eval(JSContext *ctx, const char *code)
{
    return JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_GLOBAL);
}

static JSValue cfunc_callback(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    return JS_ThrowTypeError(ctx, "from cfunc");
}

static JSValue cfuncdata_callback(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv,
                                  int magic, JSValueConst *func_data)
{
    return JS_ThrowTypeError(ctx, "from cfuncdata");
}

static JSValue cclosure_callback(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv,
                                 int magic, void *func_data)
{
  return JS_ThrowTypeError(ctx, "from cclosure");
}

static bool closure_finalized = false;

static void cclosure_opaque_finalize(void *opaque)
{
    if ((intptr_t)opaque == 12)
        closure_finalized = true;
}

static void cfunctions(void)
{
    uint32_t length;
    const char *s;
    JSValue ret, stack;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue cfunc = JS_NewCFunction(ctx, cfunc_callback, "cfunc", 42);
    JSValue cfuncdata =
        JS_NewCFunctionData2(ctx, cfuncdata_callback, "cfuncdata",
                             /*length*/1337, /*magic*/0, /*data_len*/0, NULL);
    JSValue cclosure =
        JS_NewCClosure(ctx, cclosure_callback, "cclosure", cclosure_opaque_finalize,
                       /*length*/0xC0DE, /*magic*/11, /*opaque*/(void*)12);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "cfunc", cfunc);
    JS_SetPropertyStr(ctx, global, "cfuncdata", cfuncdata);
    JS_SetPropertyStr(ctx, global, "cclosure", cclosure);
    JS_FreeValue(ctx, global);

    ret = eval(ctx, "cfunc.name");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "cfunc"));
    JS_FreeCString(ctx, s);
    ret = eval(ctx, "cfunc.length");
    assert(!JS_IsException(ret));
    assert(JS_IsNumber(ret));
    assert(0 == JS_ToUint32(ctx, &length, ret));
    assert(length == 42);

    ret = eval(ctx, "cfuncdata.name");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "cfuncdata"));
    JS_FreeCString(ctx, s);
    ret = eval(ctx, "cfuncdata.length");
    assert(!JS_IsException(ret));
    assert(JS_IsNumber(ret));
    assert(0 == JS_ToUint32(ctx, &length, ret));
    assert(length == 1337);

    ret = eval(ctx, "cclosure.name");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "cclosure"));
    JS_FreeCString(ctx, s);
    ret = eval(ctx, "cclosure.length");
    assert(!JS_IsException(ret));
    assert(JS_IsNumber(ret));
    assert(0 == JS_ToUint32(ctx, &length, ret));
    assert(length == 0xC0DE);

    ret = eval(ctx, "cfunc()");
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    stack = JS_GetPropertyStr(ctx, ret, "stack");
    assert(JS_IsString(stack));
    s = JS_ToCString(ctx, stack);
    JS_FreeValue(ctx, stack);
    assert(s);
    assert(!strcmp(s, "    at cfunc (native)\n    at <eval> (<input>:1:1)\n"));
    JS_FreeCString(ctx, s);
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "TypeError: from cfunc"));
    JS_FreeCString(ctx, s);

    ret = eval(ctx, "cfuncdata()");
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    stack = JS_GetPropertyStr(ctx, ret, "stack");
    assert(JS_IsString(stack));
    s = JS_ToCString(ctx, stack);
    JS_FreeValue(ctx, stack);
    assert(s);
    assert(!strcmp(s, "    at cfuncdata (native)\n    at <eval> (<input>:1:1)\n"));
    JS_FreeCString(ctx, s);
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "TypeError: from cfuncdata"));
    JS_FreeCString(ctx, s);

    ret = eval(ctx, "cclosure()");
    assert(JS_IsException(ret));
    ret = JS_GetException(ctx);
    assert(JS_IsError(ret));
    stack = JS_GetPropertyStr(ctx, ret, "stack");
    assert(JS_IsString(stack));
    s = JS_ToCString(ctx, stack);
    JS_FreeValue(ctx, stack);
    assert(s);
    assert(!strcmp(s, "    at cclosure (native)\n    at <eval> (<input>:1:1)\n"));
    JS_FreeCString(ctx, s);
    s = JS_ToCString(ctx, ret);
    JS_FreeValue(ctx, ret);
    assert(s);
    assert(!strcmp(s, "TypeError: from cclosure"));
    JS_FreeCString(ctx, s);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    assert(closure_finalized);
}

#define MAX_TIME 10

static int timeout_interrupt_handler(JSRuntime *rt, void *opaque)
{
    int *time = (int *)opaque;
    if (*time <= MAX_TIME)
        *time += 1;
    return *time > MAX_TIME;
}

static void sync_call(void)
{
    static const char code[] =
"(function() { \
    try { \
        while (true) {} \
    } catch (e) {} \
})();";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret = eval(ctx, code);
    assert(time > MAX_TIME);
    assert(JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_HasException(ctx));
    JSValue e = JS_GetException(ctx);
    assert(JS_IsUncatchableError(e));
    JS_FreeValue(ctx, e);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void async_call(void)
{
    static const char code[] =
"(async function() { \
    const loop = async () => { \
        await Promise.resolve(); \
        while (true) {} \
    }; \
    await loop().catch(() => {}); \
})();";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret = eval(ctx, code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_IsJobPending(rt));
    int r = 0;
    while (JS_IsJobPending(rt)) {
        r = JS_ExecutePendingJob(rt, &ctx);
    }
    assert(time > MAX_TIME);
    assert(r == -1);
    assert(JS_HasException(ctx));
    JSValue e = JS_GetException(ctx);
    assert(JS_IsUncatchableError(e));
    JS_FreeValue(ctx, e);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue save_value(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv)
{
    assert(argc == 1);
    JSValue *p = (JSValue *)JS_GetContextOpaque(ctx);
    *p = JS_DupValue(ctx, argv[0]);
    return JS_UNDEFINED;
}

static void async_call_stack_overflow(void)
{
    static const char code[] =
"(async function() { \
    const f = () => f(); \
    try { \
        await Promise.resolve(); \
        f(); \
    } catch (e) { \
        save_value(e); \
    } \
})();";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue value = JS_UNDEFINED;
    JS_SetContextOpaque(ctx, &value);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "save_value", JS_NewCFunction(ctx, save_value, "save_value", 1));
    JS_FreeValue(ctx, global);
    JSValue ret = eval(ctx, code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_IsJobPending(rt));
    int r = 0;
    while (JS_IsJobPending(rt)) {
        r = JS_ExecutePendingJob(rt, &ctx);
    }
    assert(r == 1);
    assert(!JS_HasException(ctx));
    assert(JS_IsError(value)); // stack overflow should be caught
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

// https://github.com/quickjs-ng/quickjs/issues/914
static void raw_context_global_var(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContextRaw(rt);
    JS_AddIntrinsicEval(ctx);
    {
        JSValue ret = eval(ctx, "globalThis");
        assert(JS_IsException(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        JSValue ret = eval(ctx, "var x = 42");
        assert(JS_IsUndefined(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        JSValue ret = eval(ctx, "function f() {}");
        assert(JS_IsUndefined(ret));
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void is_array(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    {
        JSValue ret = eval(ctx, "[]");
        assert(!JS_IsException(ret));
        assert(JS_IsArray(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        JSValue ret = eval(ctx, "new Proxy([], {})");
        assert(!JS_IsException(ret));
        assert(!JS_IsArray(ret));
        assert(JS_IsProxy(ret));
        JSValue handler = JS_GetProxyHandler(ctx, ret);
        JSValue target = JS_GetProxyTarget(ctx, ret);
        assert(!JS_IsException(handler));
        assert(!JS_IsException(target));
        assert(!JS_IsProxy(handler));
        assert(!JS_IsProxy(target));
        assert(JS_IsObject(handler));
        assert(JS_IsArray(target));
        JS_FreeValue(ctx, handler);
        JS_FreeValue(ctx, target);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int loader_calls;

static JSModuleDef *loader(JSContext *ctx, const char *name, void *opaque)
{
    loader_calls++;
    assert(!strcmp(name, "b"));
    static const char code[] = "export function f(x){}";
    JSValue ret = JS_Eval(ctx, code, strlen(code), "b",
                          JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(ret));
    JSModuleDef *m = JS_VALUE_GET_PTR(ret);
    assert(m);
    JS_FreeValue(ctx, ret);
    return m;
}

static void module_serde(void)
{
    JSRuntime *rt = new_runtime();
    //JS_SetDumpFlags(rt, JS_DUMP_MODULE_RESOLVE);
    JS_SetModuleLoaderFunc(rt, NULL, loader, NULL);
    JSContext *ctx = JS_NewContext(rt);
    static const char code[] = "import {f} from 'b'; f()";
    assert(loader_calls == 0);
    JSValue mod = JS_Eval(ctx, code, strlen(code), "a",
                          JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_COMPILE_ONLY);
    assert(loader_calls == 1);
    assert(!JS_IsException(mod));
    assert(JS_IsModule(mod));
    size_t len = 0;
    uint8_t *buf = JS_WriteObject(ctx, &len, mod,
                                  JS_WRITE_OBJ_BYTECODE|JS_WRITE_OBJ_REFERENCE);
    assert(buf);
    assert(len > 0);
    JS_FreeValue(ctx, mod);
    assert(loader_calls == 1);
    mod = JS_ReadObject(ctx, buf, len, JS_READ_OBJ_BYTECODE);
    js_free(ctx, buf);
    assert(loader_calls == 1); // 'b' is returned from cache
    assert(!JS_IsException(mod));
    JSValue ret = JS_EvalFunction(ctx, mod);
    assert(!JS_IsException(ret));
    assert(JS_IsPromise(ret));
    JSValue result = JS_PromiseResult(ctx, ret);
    assert(!JS_IsException(result));
    assert(JS_IsUndefined(result));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, mod);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* Tests for the async dynamic-import loader (JS_SetModuleLoaderFuncAsync etc).
   The loader simulates async I/O: it stashes the handle and enqueues a job that
   settles it on a later tick; tests pump the queue and check import()'s result. */

typedef enum {
    DELIVER_FULFILL,      /* compile `deliver_src` as a module and fulfill */
    DELIVER_FULFILL_NULL, /* fulfill with a NULL module (generic failure) */
    DELIVER_REJECT,       /* reject with an Error whose message is `deliver_src` */
    DELIVER_DOUBLE,       /* fulfill, then settle again to exercise the guard */
    DELIVER_NEVER,        /* stash the handle but never settle it */
} deliver_mode;

typedef struct {
    int loader_calls;       /* async loader invocations */
    int sync_loader_calls;  /* sync loader invocations (transitive deps) */
    int normalize_calls;    /* async normalizer invocations */
    int attrs_calls;        /* async attribute-check invocations */
    bool attrs_reject;      /* if set, the attribute check fails */
    bool reentrant;         /* if set, the loader settles synchronously */
    deliver_mode mode;
    const char *deliver_src; /* module source, or reject message */
    /* pending hand-off between the async loader and its deliver job */
    JSAsyncModuleLoadHandle handle;
    char *module_name;
} async_test_state;

static async_test_state *g_async;

/* identity normalizer that counts calls, to prove the hook is wired in */
static char *async_normalize(JSContext *ctx, const char *base_name,
                             const char *name, void *opaque)
{
    (void)base_name; (void)opaque;
    g_async->normalize_calls++;
    return js_strdup(ctx, name);
}

/* Async attribute checker: counts calls and optionally fails. */
static int async_check_attrs(JSContext *ctx, void *opaque, JSValueConst attributes)
{
    (void)opaque; (void)attributes;
    g_async->attrs_calls++;
    if (g_async->attrs_reject) {
        JS_ThrowTypeError(ctx, "unsupported import attributes");
        return -1;
    }
    return 0;
}

/* sync loader serving a tiny fixed table, for static/transitive imports */
static JSModuleDef *sync_module_loader(JSContext *ctx, const char *module_name,
                                       void *opaque)
{
    (void)opaque;
    const char *src = NULL;
    if (!strcmp(module_name, "base"))
        src = "export const base = 41;";
    else if (!strcmp(module_name, "cached"))
        src = "export const v = 7;";
    if (!src) {
        JS_ThrowReferenceError(ctx, "sync loader: unknown module '%s'",
                               module_name);
        return NULL;
    }
    if (g_async)
        g_async->sync_loader_calls++;
    JSValue mod_val = JS_Eval(ctx, src, strlen(src), module_name,
                              JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(mod_val))
        return NULL;
    JSModuleDef *m = JS_VALUE_GET_PTR(mod_val);
    JS_FreeValue(ctx, mod_val);
    return m;
}

static JSValue async_loader_deliver_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    (void)argc; (void)argv;
    JSAsyncModuleLoadHandle handle = g_async->handle;
    char *name = g_async->module_name;
    g_async->handle = NULL;
    g_async->module_name = NULL;
    assert(handle);

    if (g_async->mode == DELIVER_REJECT) {
        JSValue err = JS_NewError(ctx);
        JS_DefinePropertyValueStr(ctx, err, "message",
                                  JS_NewString(ctx, g_async->deliver_src),
                                  JS_PROP_C_W_E);
        /* JS_RejectAsyncModuleLoad takes ownership of `err`. */
        JS_RejectAsyncModuleLoad(ctx, handle, err);
        free(name);
        return JS_UNDEFINED;
    }

    if (g_async->mode == DELIVER_FULFILL_NULL) {
        /* Fulfilling with NULL means "load failed"; the engine synthesizes a
           generic rejection. */
        JS_FulfillAsyncModuleLoad(ctx, handle, NULL);
        free(name);
        return JS_UNDEFINED;
    }

    JSValue mod_val = JS_Eval(ctx, g_async->deliver_src,
                              strlen(g_async->deliver_src), name,
                              JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(mod_val));
    JSModuleDef *m = JS_VALUE_GET_PTR(mod_val);
    assert(m);
    /* The module def is owned by the runtime's module table; the JSValue
       wrapper is just a handle, so free it. */
    JS_FreeValue(ctx, mod_val);
    free(name);

    JS_FulfillAsyncModuleLoad(ctx, handle, m);
    if (g_async->mode == DELIVER_DOUBLE) {
        /* Second settle on the same handle must be a safe no-op. Try both a
           repeat fulfill and a reject; neither should change the result or
           crash. */
        JS_FulfillAsyncModuleLoad(ctx, handle, m);
        JS_RejectAsyncModuleLoad(ctx, handle, JS_NewString(ctx, "ignored"));
    }
    return JS_UNDEFINED;
}

static void async_loader(JSContext *ctx, const char *module_name,
                         void *opaque, JSValueConst attributes,
                         JSAsyncModuleLoadHandle handle)
{
    (void)opaque; (void)attributes;
    g_async->loader_calls++;
    assert(!g_async->handle); /* one outstanding at a time in these tests */
    g_async->handle = handle;
    g_async->module_name = strdup(module_name);
    if (g_async->mode == DELIVER_NEVER)
        return; /* leave the handle pending; JS_FreeRuntime must reclaim it */
    if (g_async->reentrant) {
        /* Settle synchronously, from inside the loader callback itself. */
        async_loader_deliver_job(ctx, 0, NULL);
        return;
    }
    int r = JS_EnqueueJob(ctx, async_loader_deliver_job, 0, NULL);
    assert(r == 0);
}

/* Pump the job queue to quiescence (bounded, to catch run-away loops). */
static void pump_jobs(JSRuntime *rt)
{
    int max_iters = 4096;
    while (JS_IsJobPending(rt) && max_iters-- > 0) {
        JSContext *job_ctx;
        int r = JS_ExecutePendingJob(rt, &job_ctx);
        assert(r >= 0);
    }
    assert(max_iters > 0);
}

static int32_t get_int_global(JSContext *ctx, const char *name)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue v = JS_GetPropertyStr(ctx, global, name);
    assert(JS_IsNumber(v));
    int32_t n = -1;
    JS_ToInt32(ctx, &n, v);
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, global);
    return n;
}

static char *get_string_global(JSContext *ctx, const char *name)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue v = JS_GetPropertyStr(ctx, global, name);
    assert(JS_IsString(v));
    const char *s = JS_ToCString(ctx, v);
    char *out = strdup(s);
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, global);
    return out;
}

static void run_async_eval(JSContext *ctx, const char *code)
{
    JSValue ev = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
    assert(!JS_IsException(ev));
    JS_FreeValue(ctx, ev);
}

/* fulfill: import('hello') resolves to { v: 42 }. */
static void async_module_loader(void)
{
    async_test_state st = { .mode = DELIVER_FULFILL,
                            .deliver_src = "export const v = 42;" };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('hello').then(m => { globalThis.__result = m.v; });");
    pump_jobs(rt);

    assert(st.loader_calls == 1);
    assert(!st.handle);
    assert(get_int_global(ctx, "__result") == 42);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* reject: import('boom') is caught with our error message. */
static void async_module_loader_reject(void)
{
    async_test_state st = { .mode = DELIVER_REJECT, .deliver_src = "kaboom" };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('boom').then(() => { globalThis.__err = 'resolved?!'; },"
        "                    e => { globalThis.__err = e.message; });");
    pump_jobs(rt);

    assert(st.loader_calls == 1);
    char *err = get_string_global(ctx, "__err");
    assert(!strcmp(err, "kaboom"));
    free(err);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* transitive: an async-loaded module statically imports 'base'; that static
   dependency must be resolved through the SYNC loader. */
static void async_module_loader_transitive(void)
{
    async_test_state st = { .mode = DELIVER_FULFILL,
                            .deliver_src = "import { base } from 'base';"
                                           "export const v = base + 1;" };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFunc(rt, NULL, sync_module_loader, NULL);
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('combo').then(m => { globalThis.__result = m.v; });");
    pump_jobs(rt);

    assert(st.loader_calls == 1);      /* 'combo' went through the async loader */
    assert(st.sync_loader_calls == 1); /* 'base' went through the sync loader  */
    assert(get_int_global(ctx, "__result") == 42);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* cache hit: a specifier already in the module table (loaded via static
   import through the sync loader) is settled from cache, NOT via the loader. */
static void async_module_loader_cache_hit(void)
{
    async_test_state st = { .mode = DELIVER_FULFILL,
                            .deliver_src = "export const v = 999;" /* must NOT be used */ };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFunc(rt, NULL, sync_module_loader, NULL);
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    /* Statically import 'cached' so it's resolved + in the module table, then
       dynamically import the same specifier. */
    run_async_eval(ctx,
        "import { v } from 'cached';"
        "import('cached').then(m => { globalThis.__result = m.v; });");
    pump_jobs(rt);

    assert(st.sync_loader_calls == 1); /* only the static import hit the loader */
    assert(st.loader_calls == 0);      /* the dynamic import was a cache hit    */
    assert(!st.handle);
    assert(get_int_global(ctx, "__result") == 7);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* double settle: a second fulfill/reject on the same handle is a no-op. */
static void async_module_loader_double_settle(void)
{
    async_test_state st = { .mode = DELIVER_DOUBLE,
                            .deliver_src = "export const v = 5;" };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('twice').then(m => { globalThis.__result = m.v; },"
        "                     () => { globalThis.__result = -1; });");
    pump_jobs(rt);

    assert(st.loader_calls == 1);
    /* The first fulfill wins; the redundant fulfill/reject are ignored. */
    assert(get_int_global(ctx, "__result") == 5);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* shutdown sweep: the host never settles the handle. JS_FreeRuntime must
   reclaim it without leaking (new_runtime aborts on leaks) or crashing. */
static void async_module_loader_shutdown_pending(void)
{
    async_test_state st = { .mode = DELIVER_NEVER, .deliver_src = NULL };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx, "import('never').then(() => {}, () => {});");
    pump_jobs(rt);

    assert(st.loader_calls == 1);
    assert(st.handle);          /* still pending, deliberately never settled */
    free(st.module_name);       /* the deliver job that would free it never ran */
    st.module_name = NULL;

    /* The pending handle (and the references it owns) must be reclaimed here. */
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* top-level await: the loaded module's top-level `await` must complete before
   the import() Promise settles. */
static void async_module_loader_tla(void)
{
    async_test_state st = { .mode = DELIVER_FULFILL,
                            .deliver_src = "export const v = await Promise.resolve(123);" };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('tla').then(m => { globalThis.__result = m.v; },"
        "                   e => { globalThis.__result = -1; });");
    pump_jobs(rt);

    assert(st.loader_calls == 1);
    assert(get_int_global(ctx, "__result") == 123);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* re-entrant settle: the host may call JS_FulfillAsyncModuleLoad synchronously
   from inside the loader callback (load already cached/available). */
static void async_module_loader_reentrant(void)
{
    async_test_state st = { .mode = DELIVER_FULFILL, .reentrant = true,
                            .deliver_src = "export const v = 7;" };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('re').then(m => { globalThis.__result = m.v; },"
        "                  e => { globalThis.__result = -1; });");
    pump_jobs(rt);

    assert(st.loader_calls == 1);
    assert(get_int_global(ctx, "__result") == 7);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* NULL module: fulfilling with a NULL JSModuleDef rejects the import() Promise
   with a synthesized generic error. */
static void async_module_loader_null_module(void)
{
    async_test_state st = { .mode = DELIVER_FULFILL_NULL };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('n').then(() => { globalThis.__err = 'resolved?!'; },"
        "                 e => { globalThis.__err = e.message; });");
    pump_jobs(rt);

    char *err = get_string_global(ctx, "__err");
    assert(!strcmp(err, "async module load returned NULL"));
    free(err);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* evaluation throw: a module that throws at top level rejects import() with
   the thrown error (the loader/compile succeeded; evaluation failed). */
static void async_module_loader_eval_throw(void)
{
    async_test_state st = { .mode = DELIVER_FULFILL,
                            .deliver_src = "throw new Error('boom-in-module');"
                                           "export const v = 1;" };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('t').then(() => { globalThis.__err = 'resolved?!'; },"
        "                 e => { globalThis.__err = e.message; });");
    pump_jobs(rt);

    char *err = get_string_global(ctx, "__err");
    assert(!strcmp(err, "boom-in-module"));
    free(err);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* attribute check: a registered async attribute checker that rejects must
   reject the import() BEFORE the loader is ever invoked. */
static void async_module_loader_attrs_reject(void)
{
    async_test_state st = { .mode = DELIVER_FULFILL, .attrs_reject = true,
                            .deliver_src = "export const v = 1;" };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, async_loader, async_check_attrs, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('x', { with: { type: 'json' } })"
        "  .then(() => { globalThis.__err = 'resolved?!'; },"
        "        e => { globalThis.__err = e.message; });");
    pump_jobs(rt);

    assert(st.attrs_calls == 1);
    assert(st.loader_calls == 0); /* rejected before the loader ran */
    char *err = get_string_global(ctx, "__err");
    assert(!strcmp(err, "unsupported import attributes"));
    free(err);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* normalizer: a registered async normalizer must be invoked to resolve the
   specifier before the loader is called. */
static void async_module_loader_normalize(void)
{
    async_test_state st = { .mode = DELIVER_FULFILL,
                            .deliver_src = "export const v = 55;" };
    g_async = &st;

    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, async_normalize, async_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('norm').then(m => { globalThis.__result = m.v; },"
        "                    e => { globalThis.__result = -1; });");
    pump_jobs(rt);

    assert(st.normalize_calls >= 1);
    assert(st.loader_calls == 1);
    assert(get_int_global(ctx, "__result") == 55);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    g_async = NULL;
}

/* nested dynamic import: an async-loaded module's own import() also routes
   through the async loader (loader called for 'outer' then 'inner'). */
#define NESTED_Q 8
static JSAsyncModuleLoadHandle nested_hq[NESTED_Q];
static char *nested_nq[NESTED_Q];
static int nested_qn;
static int nested_loader_calls;

static JSValue nested_deliver_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    (void)argc; (void)argv;
    if (nested_qn == 0)
        return JS_UNDEFINED;
    JSAsyncModuleLoadHandle handle = nested_hq[0];
    char *name = nested_nq[0];
    for (int i = 1; i < nested_qn; i++) {
        nested_hq[i-1] = nested_hq[i]; nested_nq[i-1] = nested_nq[i];
    }
    nested_qn--;
    const char *src = !strcmp(name, "outer")
        ? "const inner = await import('inner'); export const v = inner.w + 1;"
        : "export const w = 40;";
    JSValue mv = JS_Eval(ctx, src, strlen(src), name,
                         JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(mv));
    JSModuleDef *m = JS_VALUE_GET_PTR(mv);
    JS_FreeValue(ctx, mv);
    free(name);
    JS_FulfillAsyncModuleLoad(ctx, handle, m);
    return JS_UNDEFINED;
}

static void nested_loader(JSContext *ctx, const char *module_name, void *opaque,
                          JSValueConst attributes, JSAsyncModuleLoadHandle handle)
{
    (void)opaque; (void)attributes;
    nested_loader_calls++;
    assert(nested_qn < NESTED_Q);
    nested_hq[nested_qn] = handle;
    nested_nq[nested_qn] = strdup(module_name);
    nested_qn++;
    int r = JS_EnqueueJob(ctx, nested_deliver_job, 0, NULL);
    assert(r == 0);
}

static void async_module_loader_nested_dynamic(void)
{
    nested_qn = 0; nested_loader_calls = 0;
    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, nested_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "import('outer').then(m => { globalThis.__result = m.v; },"
        "                     e => { globalThis.__result = -1; });");
    pump_jobs(rt);

    assert(nested_loader_calls == 2); /* outer + inner both via the async loader */
    assert(get_int_global(ctx, "__result") == 41);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* concurrency harness: many loads in flight at once. The loaded module bumps a
   global eval counter so tests can detect double-evaluation (broken dedup). */
#define ASYNC_Q 128
static JSAsyncModuleLoadHandle cc_hq[ASYNC_Q];
static char *cc_nq[ASYNC_Q];
static int cc_qn;
static int cc_loader_calls;
static int cc_never; /* if set, never enqueue delivery (leave handles pending) */

static JSValue cc_deliver(JSContext *ctx, int argc, JSValueConst *argv)
{
    (void)argc; (void)argv;
    if (cc_qn == 0)
        return JS_UNDEFINED;
    JSAsyncModuleLoadHandle h = cc_hq[0];
    char *name = cc_nq[0];
    for (int i = 1; i < cc_qn; i++) { cc_hq[i-1] = cc_hq[i]; cc_nq[i-1] = cc_nq[i]; }
    cc_qn--;
    static const char src[] =
        "globalThis.__evals = (globalThis.__evals||0) + 1; export const v = 9;";
    JSValue mv = JS_Eval(ctx, src, strlen(src), name,
                         JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(mv));
    JSModuleDef *m = JS_VALUE_GET_PTR(mv);
    JS_FreeValue(ctx, mv);
    free(name);
    JS_FulfillAsyncModuleLoad(ctx, h, m);
    return JS_UNDEFINED;
}

static void cc_loader(JSContext *ctx, const char *module_name, void *opaque,
                      JSValueConst attributes, JSAsyncModuleLoadHandle handle)
{
    (void)opaque; (void)attributes;
    cc_loader_calls++;
    assert(cc_qn < ASYNC_Q);
    cc_hq[cc_qn] = handle;
    cc_nq[cc_qn] = strdup(module_name);
    cc_qn++;
    if (cc_never)
        return;
    int r = JS_EnqueueJob(ctx, cc_deliver, 0, NULL);
    assert(r == 0);
}

/* in-flight dedup: two concurrent import()s of the same specifier must invoke
   the loader once, evaluate once, and resolve both to the same namespace. */
static void async_module_loader_inflight_dedup(void)
{
    cc_qn = 0; cc_loader_calls = 0; cc_never = 0;
    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, cc_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "globalThis.__evals = 0; globalThis.__same = 0;"
        "Promise.all([import('dup'), import('dup')]).then(([a, b]) => {"
        "  globalThis.__same = (a === b) ? 1 : 0; });");
    pump_jobs(rt);

    assert(cc_loader_calls == 1);                 /* loader invoked once */
    assert(get_int_global(ctx, "__evals") == 1);  /* module evaluated once */
    assert(get_int_global(ctx, "__same") == 1);   /* identical namespace */

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* the dual of dedup: many distinct specifiers in flight must each load and
   evaluate exactly once (dedup must not be over-eager). */
static void async_module_loader_concurrent_distinct(void)
{
    cc_qn = 0; cc_loader_calls = 0; cc_never = 0;
    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, cc_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "globalThis.__evals = 0; globalThis.__n = 0;"
        "{ const a = []; for (let i = 0; i < 64; i++)"
        "    a.push(import('m' + i).then(m => { globalThis.__n += m.v; })); }");
    pump_jobs(rt);

    assert(cc_loader_calls == 64);
    assert(get_int_global(ctx, "__evals") == 64);
    assert(get_int_global(ctx, "__n") == 64 * 9);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* shutdown must reclaim a handle with multiple waiters: several concurrent
   imports of one never-settled specifier share a handle JS_FreeRuntime frees. */
static void async_module_loader_shutdown_multi_waiter(void)
{
    cc_qn = 0; cc_loader_calls = 0; cc_never = 1;
    JSRuntime *rt = new_runtime();
    JS_SetModuleLoaderFuncAsync(rt, NULL, cc_loader, NULL, NULL);
    JSContext *ctx = JS_NewContext(rt);

    run_async_eval(ctx,
        "Promise.all([import('z'), import('z'), import('z')]).then(()=>{},()=>{});");
    pump_jobs(rt);

    assert(cc_loader_calls == 1); /* one handle, three waiters attached to it */
    for (int i = 0; i < cc_qn; i++)
        free(cc_nq[i]);           /* delivery never ran, so free the stashed names */

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

struct rejection_counts {
    int reject_count;
    int handle_count;
};

static void rejection_counter(JSContext *ctx, JSValueConst promise,
                              JSValueConst reason, bool is_handled, void *opaque)
{
    struct rejection_counts *c = opaque;
    if (is_handled)
        c->handle_count++;
    else
        c->reject_count++;
}

// A synchronous module that throws at top level must surface exactly one unhandled rejection
static void module_unhandled_rejection(void)
{
    struct rejection_counts c = {0, 0};
    JSRuntime *rt = new_runtime();
    JS_SetHostPromiseRejectionTracker(rt, rejection_counter, &c);
    JSContext *ctx = JS_NewContext(rt);

    static const char code[] = "throw new Error('Nuke the entire site from orbit. It\\'s the only way to be sure.')";
    JSValue v = JS_Eval(ctx, code, strlen(code), "<m>", JS_EVAL_TYPE_MODULE);
    JS_FreeValue(ctx, v);

    JSContext *c1;
    while (JS_ExecutePendingJob(rt, &c1) > 0)
        ;

    // net unhandled rejections == (2 rejects - 1 handled)
    assert(c.reject_count == 2);
    assert(c.handle_count == 1);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void runtime_cstring_free(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    // string -> cstring + JS_FreeCStringRT
    {
        JSValue ret = eval(ctx, "\"testStringPleaseIgnore\"");
        assert(JS_IsString(ret));
        const char *s = JS_ToCString(ctx, ret);
        assert(s);
        assert(strcmp(s, "testStringPleaseIgnore") == 0);
        JS_FreeCStringRT(rt, s);
        JS_FreeValue(ctx, ret);
    }
    // string -> cstring + JS_FreeCStringRT, destroying the source value first
    {
        JSValue ret = eval(ctx, "\"testStringPleaseIgnore\"");
        assert(JS_IsString(ret));
        const char *s = JS_ToCString(ctx, ret);
        assert(s);
        JS_FreeValue(ctx, ret);
        assert(strcmp(s, "testStringPleaseIgnore") == 0);
        JS_FreeCStringRT(rt, s);
    }
    // number -> cstring + JS_FreeCStringRT
    {
        JSValue ret = eval(ctx, "123987");
        assert(JS_IsNumber(ret));
        const char *s = JS_ToCString(ctx, ret);
        assert(s);
        assert(strcmp(s, "123987") == 0);
        JS_FreeCStringRT(rt, s);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void utf16_string(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    {
        JSValue v = JS_NewStringUTF16(ctx, NULL, 0);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        assert(!strcmp(s, ""));
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringUTF16(ctx, (uint16_t[]){'o','k'}, 2);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        assert(!strcmp(s, "ok"));
        JS_FreeCString(ctx, s);
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 2);
        assert(u[0] == 'o');
        assert(u[1] == 'k');
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringUTF16(ctx, (uint16_t[]){0xD800}, 1);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        // questionable but surrogates don't map to UTF-8 without WTF-8
        assert(!strcmp(s, "\xED\xA0\x80"));
        JS_FreeCString(ctx, s);
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 1);
        assert(u[0] == 0xD800);
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringLen(ctx, "ok", 2); // ascii -> ucs
        assert(!JS_IsException(v));
        size_t n;
        const uint16_t *u = JS_ToCStringLenUTF16(ctx, &n, v);
        assert(u);
        assert(n == 2);
        assert(u[0] == 'o');
        assert(u[1] == 'k');
        JS_FreeCStringUTF16(ctx, u);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewStringUTF16(ctx, NULL, (size_t)INT_MAX + 1);
        assert(JS_IsException(v));
        JSValue e = JS_GetException(ctx);
        assert(JS_IsError(e));
        const char *s = JS_ToCString(ctx, e);
        assert(s);
        assert(strstr(s, "invalid string length") != NULL);
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, e);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void weak_map_gc_check(void)
{
    static const char init_code[] =
"const map = new WeakMap(); \
function addItem() { \
    const k = { \
        text: 'a', \
    }; \
    map.set(k, {k}); \
}";
    static const char test_code[] = "addItem()";

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue ret = eval(ctx, init_code);
    assert(!JS_IsException(ret));

    JSValue ret_test = eval(ctx, test_code);
    assert(!JS_IsException(ret_test));
    JS_RunGC(rt);
    JSMemoryUsage memory_usage;
    JS_ComputeMemoryUsage(rt, &memory_usage);

    for (int i = 0; i < 3; i++) {
        JSValue ret_test2 = eval(ctx, test_code);
        assert(!JS_IsException(ret_test2));
        JS_RunGC(rt);
        JSMemoryUsage memory_usage2;
        JS_ComputeMemoryUsage(rt, &memory_usage2);

        assert(memory_usage.memory_used_count == memory_usage2.memory_used_count);
        assert(memory_usage.memory_used_size == memory_usage2.memory_used_size);
        JS_FreeValue(ctx, ret_test2);
    }

    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, ret_test);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

struct {
    int hook_type_call_count[4];
} promise_hook_state;

static void promise_hook_cb(JSContext *ctx, JSPromiseHookType type,
                            JSValueConst promise, JSValueConst parent_promise,
                            void *opaque)
{
    assert(type == JS_PROMISE_HOOK_INIT ||
           type == JS_PROMISE_HOOK_BEFORE ||
           type == JS_PROMISE_HOOK_AFTER ||
           type == JS_PROMISE_HOOK_RESOLVE);
    promise_hook_state.hook_type_call_count[type]++;
    assert(opaque == (void *)&promise_hook_state);
    if (!JS_IsUndefined(parent_promise)) {
        JSValue global_object = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global_object, "actual",
                          JS_DupValue(ctx, parent_promise));
        JS_FreeValue(ctx, global_object);
    }
}

static void promise_hook(void)
{
    int *cc = promise_hook_state.hook_type_call_count;
    JSContext *unused;
    JSRuntime *rt = new_runtime();
    //JS_SetDumpFlags(rt, JS_DUMP_PROMISE);
    JS_SetPromiseHook(rt, promise_hook_cb, &promise_hook_state);
    JSContext *ctx = JS_NewContext(rt);
    JSValue global_object = JS_GetGlobalObject(ctx);
    {
        // empty module; creates an outer and inner module promise;
        // JS_Eval returns the outer promise
        JSValue ret = JS_Eval(ctx, "", 0, "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret));
        JS_FreeValue(ctx, ret);
        assert(2 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with unresolved promise; the outer and inner module promises
        // are resolved but not the user's promise
        static const char code[] = "new Promise(() => {})";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]); // outer and inner module promise
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with resolved promise
        static const char code[] = "new Promise((resolve,reject) => resolve())";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(3 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with rejected promise
        static const char code[] = "new Promise((resolve,reject) => reject())";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with promise chain
        static const char code[] =
            "globalThis.count = 0;"
            "globalThis.actual = undefined;" // set by promise_hook_cb
            "globalThis.expected = new Promise(resolve => resolve());"
            "expected.then(_ => count++)";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(4 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(3 == cc[JS_PROMISE_HOOK_RESOLVE]);
        JSValue v = JS_GetPropertyStr(ctx, global_object, "count");
        assert(!JS_IsException(v));
        int32_t count;
        assert(0 == JS_ToInt32(ctx, &count, v));
        assert(0 == count);
        JS_FreeValue(ctx, v);
        assert(JS_IsJobPending(rt));
        assert(1 == JS_ExecutePendingJob(rt, &unused));
        assert(!JS_HasException(ctx));
        assert(4 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(4 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
        v = JS_GetPropertyStr(ctx, global_object, "count");
        assert(!JS_IsException(v));
        assert(0 == JS_ToInt32(ctx, &count, v));
        assert(1 == count);
        JS_FreeValue(ctx, v);
        JSValue actual = JS_GetPropertyStr(ctx, global_object, "actual");
        JSValue expected = JS_GetPropertyStr(ctx, global_object, "expected");
        assert(!JS_IsException(actual));
        assert(!JS_IsException(expected));
        assert(JS_IsSameValue(ctx, actual, expected));
        JS_FreeValue(ctx, actual);
        JS_FreeValue(ctx, expected);
    }
    memset(&promise_hook_state, 0, sizeof(promise_hook_state));
    {
        // module with thenable; fires before and after hooks
        static const char code[] =
            "new Promise(resolve => resolve({then(resolve){ resolve() }}))";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_MODULE);
        assert(!JS_IsException(ret));
        assert(JS_IsPromise(ret));
        assert(JS_PROMISE_FULFILLED == JS_PromiseState(ctx, ret)); // outer module promise
        JS_FreeValue(ctx, ret);
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(0 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(0 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(2 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(JS_IsJobPending(rt));
        assert(1 == JS_ExecutePendingJob(rt, &unused));
        assert(!JS_HasException(ctx));
        assert(3 == cc[JS_PROMISE_HOOK_INIT]);
        assert(1 == cc[JS_PROMISE_HOOK_BEFORE]);
        assert(1 == cc[JS_PROMISE_HOOK_AFTER]);
        assert(3 == cc[JS_PROMISE_HOOK_RESOLVE]);
        assert(!JS_IsJobPending(rt));
    }
    JS_FreeValue(ctx, global_object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void dump_memory_usage(void)
{
    JSMemoryUsage stats;

    JSRuntime *rt = NULL;
    JSContext *ctx = NULL;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    //JS_SetDumpFlags(rt, JS_DUMP_PROMISE);

    static const char code[] =
    "globalThis.count = 0;"
    "globalThis.actual = undefined;" // set by promise_hook_cb
    "globalThis.expected = new Promise(resolve => resolve());"
    "expected.then(_ => count++)";

    JSValue evalVal = JS_Eval(ctx, code, strlen(code), "<input>", 0);
    JS_FreeValue(ctx, evalVal);

    FILE *temp = tmpfile();
    assert(temp != NULL);
    JS_ComputeMemoryUsage(rt, &stats);
    JS_DumpMemoryUsage(temp, &stats, rt);
    // JS_DumpMemoryUsage(stdout, &stats, rt);
    fclose(temp);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void new_errors(void)
{
    typedef struct {
        const char name[16];
        JSValue (*func)(JSContext *, const char *, ...);
    } Entry;
    static const Entry entries[] = {
        {"Error",           JS_NewPlainError},
        {"InternalError",   JS_NewInternalError},
        {"RangeError",      JS_NewRangeError},
        {"ReferenceError",  JS_NewReferenceError},
        {"SyntaxError",     JS_NewSyntaxError},
        {"TypeError",       JS_NewTypeError},
    };
    const Entry *e;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    for (e = entries; e < endof(entries); e++) {
        JSValue obj = (*e->func)(ctx, "the %s", "needle");
        assert(!JS_IsException(obj));
        assert(JS_IsObject(obj));
        assert(JS_IsError(obj));
        const char *haystack = JS_ToCString(ctx, obj);
        char needle[256];
        snprintf(needle, sizeof(needle), "%s: the needle", e->name);
        assert(strstr(haystack, needle));
        JS_FreeCString(ctx, haystack);
        JSValue stack = JS_GetPropertyStr(ctx, obj, "stack");
        assert(!JS_IsException(stack));
        assert(JS_IsString(stack));
        JS_FreeValue(ctx, stack);
        JS_FreeValue(ctx, obj);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void backtrace_oom_current_exception(void)
{
    static const char setup_code[] =
        "globalThis.f = function() { missing; };\n"
        "Object.defineProperty(f, 'name', { value: 'x'.repeat(2 * 1024 * 1024) });";
    JSMemoryUsage stats;
    JSValue ret, exception;
    JSRuntime *rt;
    JSContext *ctx;

    rt = new_runtime();
    ctx = JS_NewContext(rt);

    ret = eval(ctx, setup_code);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    JS_ComputeMemoryUsage(rt, &stats);
    JS_SetMemoryLimit(rt, (size_t)stats.malloc_size + 128 * 1024);

    ret = eval(ctx, "f()");
    assert(JS_IsException(ret));
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_SetMemoryLimit(rt, 0);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int gop_get_own_property(JSContext *ctx, JSPropertyDescriptor *desc,
                                JSValueConst obj, JSAtom prop)
{
    const char *name;
    int found;

    found = 0;
    name = JS_AtomToCString(ctx, prop);
    if (!name)
        return -1;
    if (!strcmp(name, "answer")) {
        found = 1;
        *desc = (JSPropertyDescriptor){
            .value = JS_NewInt32(ctx, 42),
            .flags = JS_PROP_C_W_E | JS_PROP_HAS_VALUE,
        };
    }
    JS_FreeCString(ctx, name);
    return found;
}

static void global_object_prototype(void)
{
    static const char code[] = "answer";
    JSValue global_object, proto, ret;
    JSClassID class_id;
    JSRuntime *rt;
    JSContext *ctx;
    int32_t answer;
    int res;

    {
        rt = new_runtime();
        ctx = JS_NewContext(rt);
        proto = JS_NewObject(ctx);
        assert(JS_IsObject(proto));
        JSCFunctionListEntry prop = JS_PROP_INT32_DEF("answer", 42, JS_PROP_C_W_E);
        JS_SetPropertyFunctionList(ctx, proto, &prop, 1);
        global_object = JS_GetGlobalObject(ctx);
        res = JS_SetPrototype(ctx, global_object, proto);
        assert(res == true);
        JS_FreeValue(ctx, global_object);
        JS_FreeValue(ctx, proto);
        ret = eval(ctx, code);
        assert(!JS_IsException(ret));
        assert(JS_IsNumber(ret));
        res = JS_ToInt32(ctx, &answer, ret);
        assert(res == 0);
        assert(answer == 42);
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
    {
        JSClassExoticMethods exotic = (JSClassExoticMethods){
            .get_own_property = gop_get_own_property,
        };
        JSClassDef def = (JSClassDef){
            .class_name = "Global Object",
            .exotic = &exotic,
        };
        rt = new_runtime();
        class_id = 0;
        JS_NewClassID(rt, &class_id);
        res = JS_NewClass(rt, class_id, &def);
        assert(res == 0);
        ctx = JS_NewContext(rt);
        proto = JS_NewObjectClass(ctx, class_id);
        assert(JS_IsObject(proto));
        global_object = JS_GetGlobalObject(ctx);
        res = JS_SetPrototype(ctx, global_object, proto);
        assert(res == true);
        JS_FreeValue(ctx, global_object);
        JS_FreeValue(ctx, proto);
        ret = eval(ctx, code);
        assert(!JS_IsException(ret));
        assert(JS_IsNumber(ret));
        res = JS_ToInt32(ctx, &answer, ret);
        assert(res == 0);
        assert(answer == 42);
        JS_FreeValue(ctx, ret);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
}

// https://github.com/quickjs-ng/quickjs/issues/1178
static void slice_string_tocstring(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret = eval(ctx, "'.'.repeat(16384).slice(1, -1)");
    assert(!JS_IsException(ret));
    assert(JS_IsString(ret));
    const char *str = JS_ToCString(ctx, ret);
    assert(strlen(str) == 16382);
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void immutable_array_buffer(void)
{
    JSValue obj, ret;
    bool immutable;
    char buf[96];
    int i, v;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    for (i = 0; i < 2; i++) {
        obj = JS_NewObject(ctx);
        immutable = (i == 0);
        assert(-1 == JS_IsImmutableArrayBuffer(JS_NULL));
        assert(-1 == JS_IsImmutableArrayBuffer(JS_UNDEFINED));
        assert(-1 == JS_IsImmutableArrayBuffer(obj));
        assert(-1 == JS_SetImmutableArrayBuffer(JS_NULL, immutable));
        assert(-1 == JS_SetImmutableArrayBuffer(JS_UNDEFINED, immutable));
        assert(-1 == JS_SetImmutableArrayBuffer(obj, immutable));
        JS_FreeValue(ctx, obj);
    }
    obj = eval(ctx, "globalThis.ab = new ArrayBuffer(1)");
    assert(!JS_IsException(obj));
    assert(JS_IsArrayBuffer(obj));
    assert(!JS_IsImmutableArrayBuffer(obj));
    for (i = 1; i <= 3; i++) {
        immutable = (i == 2);
        if (i > 1)
            JS_SetImmutableArrayBuffer(obj, immutable);
        assert(immutable == JS_IsImmutableArrayBuffer(obj));
        snprintf(buf, sizeof(buf),
                 "var ta = new Uint8Array(ab); ta[0] = %d; ta[0]", i);
        ret = eval(ctx, buf);
        assert(!JS_IsException(ret));
        assert(JS_IsNumber(ret));
        assert(0 == JS_ToInt32(ctx, &v, ret));
        JS_FreeValue(ctx, ret);
        if (immutable) {
            assert(v != i);
        } else {
            assert(v == i);
        }
    }
    JS_FreeValue(ctx, obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void *sab_test_alloc(void *opaque, size_t size)
{
    return malloc(size);
}

static void sab_test_free(void *opaque, void *ptr)
{
    free(ptr);
}

static void shared_array_buffer_growth(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue ret, exception;

    ret = eval(ctx, "new SharedArrayBuffer(16)");
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);

    ret = eval(ctx,
               "const sab = new SharedArrayBuffer(16, { maxByteLength: 16 });"
               "sab.grow(16);"
               "sab.byteLength === 16 && sab.maxByteLength === 16");
    assert(!JS_IsException(ret));
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    ret = eval(ctx, "new SharedArrayBuffer(16, { maxByteLength: 16384 })");
    assert(JS_IsException(ret));
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsError(exception));
    JS_FreeValue(ctx, exception);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    JSSharedArrayBufferFunctions funcs = {
        .sab_alloc = sab_test_alloc,
        .sab_free = sab_test_free,
        .sab_dup = NULL,
        .sab_opaque = NULL,
    };

    rt = new_runtime();
    JS_SetSharedArrayBufferFunctions(rt, &funcs);
    ctx = JS_NewContext(rt);
    ret = eval(ctx,
               "const sab = new SharedArrayBuffer(16, { maxByteLength: 16384 });"
               "const u8 = new Uint8Array(sab);"
               "sab.grow(16384);"
               "u8[1024] === 0 && u8.byteLength === 16384");
    assert(!JS_IsException(ret));
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void get_uint8array(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue val;
    uint8_t *p;
    size_t size;
    uint8_t buf[3] = { 1, 2, 3 };

    val = eval(ctx, "new Uint8Array(0)");
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    val = JS_NewUint8Array(ctx, NULL, 0, NULL, NULL, false);
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    val = JS_NewUint8ArrayCopy(ctx, NULL, 0);
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    val = JS_NewUint8ArrayCopy(ctx, buf, sizeof(buf));
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 3);
    assert(p[0] == 1 && p[1] == 2 && p[2] == 3);
    JS_FreeValue(ctx, val);

    val = eval(ctx, "new Uint8Array([4, 5, 6])");
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p != NULL);
    assert(size == 3);
    assert(p[0] == 4 && p[1] == 5 && p[2] == 6);
    JS_FreeValue(ctx, val);

    val = eval(ctx, "new Int32Array(4)");
    assert(!JS_IsException(val));
    p = JS_GetUint8Array(ctx, &size, val);
    assert(p == NULL);
    assert(size == 0);
    JS_FreeValue(ctx, val);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void new_symbol(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue sym, ret;

    /* Local symbol with NULL description -> Symbol() */
    sym = JS_NewSymbol(ctx, NULL, false);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_local_null", sym);

    ret = eval(ctx, "typeof sym_local_null === 'symbol' && sym_local_null.description === undefined && Symbol.keyFor(sym_local_null) === undefined");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    /* Global symbol with NULL description -> Symbol.for() -> Symbol.for('undefined') */
    sym = JS_NewSymbol(ctx, NULL, true);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_global_null", sym);

    ret = eval(ctx, "typeof sym_global_null === 'symbol' && sym_global_null.description === 'undefined' && Symbol.keyFor(sym_global_null) === 'undefined'");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    /* Local symbol with description -> Symbol('test_local') */
    sym = JS_NewSymbol(ctx, "test_local", false);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_local_str", sym);

    ret = eval(ctx, "sym_local_str.description === 'test_local' && Symbol.keyFor(sym_local_str) === undefined");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    /* Global symbol with description -> Symbol.for('test_global') */
    sym = JS_NewSymbol(ctx, "test_global", true);
    assert(!JS_IsException(sym));
    assert(JS_IsSymbol(sym));
    JS_SetPropertyStr(ctx, global, "sym_global_str", sym);

    ret = eval(ctx, "sym_global_str.description === 'test_global' && Symbol.keyFor(sym_global_str) === 'test_global'");
    assert(JS_IsBool(ret));
    assert(JS_VALUE_GET_BOOL(ret));
    JS_FreeValue(ctx, ret);

    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void bulk_free_macros(void) {
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue val0 = JS_NewObject(ctx);
    JSValue val1 = JS_NewArray(ctx);
    JSValue val2 = JS_NewDate(ctx, 0.0);
    
    JSMemoryUsage mem_usage;
    JS_ComputeMemoryUsage(rt, &mem_usage);
    int obj_count = mem_usage.obj_count;

    JS_FreeValues(ctx, val0, val1);
    JS_FreeValuesRT(rt, val2);

    // silly atoms to ensure qjs doesn't find built-ins that match
    JSAtom atom0 = JS_NewAtom(ctx, "ALL!!");
    JSAtom atom1 = JS_NewAtom(ctx, "YOUR!!");
    JSAtom atom2 = JS_NewAtom(ctx, "ATOMS!!");
    JSAtom atom3 = JS_NewAtom(ctx, "ARE!!");
    JSAtom atom4 = JS_NewAtom(ctx, "BELONG!!");
    JSAtom atom5 = JS_NewAtom(ctx, "TO US!!");

    JS_ComputeMemoryUsage(rt, &mem_usage);
    assert((obj_count - 3) == mem_usage.obj_count);
    int atom_count = mem_usage.atom_count;

    JS_FreeAtoms(ctx, atom1, atom3, atom4);
    JS_FreeAtomsRT(rt, atom0, atom2, atom5);

    JS_ComputeMemoryUsage(rt, &mem_usage);
    assert((atom_count - 6) == mem_usage.atom_count);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int detach_free_count;

static void detach_free_func(JSRuntime *rt, void *opaque, void *ptr)
{
    detach_free_count++;
    free(ptr);
}

static void detach_array_buffer_free_once(void)
{
    JSValue obj;
    uint8_t *buf;

    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    detach_free_count = 0;
    buf = malloc(8);
    obj = JS_NewArrayBuffer(ctx, buf, 8, detach_free_func, NULL, false);
    assert(JS_IsArrayBuffer(obj));

    /* detaching releases the backing store exactly once */
    JS_DetachArrayBuffer(ctx, obj);
    assert(detach_free_count == 1);

    /* finalizing the detached buffer must not release it a second time */
    JS_FreeValue(ctx, obj);
    assert(detach_free_count == 1);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    cfunctions();
    sync_call();
    async_call();
    async_call_stack_overflow();
    raw_context_global_var();
    is_array();
    module_serde();
    async_module_loader();
    async_module_loader_reject();
    async_module_loader_transitive();
    async_module_loader_cache_hit();
    async_module_loader_double_settle();
    async_module_loader_shutdown_pending();
    async_module_loader_tla();
    async_module_loader_reentrant();
    async_module_loader_null_module();
    async_module_loader_eval_throw();
    async_module_loader_attrs_reject();
    async_module_loader_normalize();
    async_module_loader_nested_dynamic();
    async_module_loader_inflight_dedup();
    async_module_loader_concurrent_distinct();
    async_module_loader_shutdown_multi_waiter();
    module_unhandled_rejection();
    runtime_cstring_free();
    utf16_string();
    weak_map_gc_check();
    promise_hook();
    dump_memory_usage();
    new_errors();
    backtrace_oom_current_exception();
    global_object_prototype();
    slice_string_tocstring();
    immutable_array_buffer();
    shared_array_buffer_growth();
    get_uint8array();
    new_symbol();
    bulk_free_macros();
    detach_array_buffer_free_once();
    return 0;
}
