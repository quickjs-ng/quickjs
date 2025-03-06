#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

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
    const char *code =
"(function() { \
    try { \
        while (true) {} \
    } catch (e) {} \
})();";

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_GLOBAL);
    assert(time > MAX_TIME);
    assert(JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_HasException(ctx));
    JSValue e = JS_GetException(ctx);
    assert(JS_IsUncatchableError(ctx, e));
    JS_FreeValue(ctx, e);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void async_call(void)
{
    const char *code =
"(async function() { \
    const loop = async () => { \
        await Promise.resolve(); \
        while (true) {} \
    }; \
    await loop().catch(() => {}); \
})();";

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    int time = 0;
    JS_SetInterruptHandler(rt, timeout_interrupt_handler, &time);
    JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_GLOBAL);
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
    assert(JS_IsUncatchableError(ctx, e));
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
    const char *code =
"(async function() { \
    const f = () => f(); \
    try { \
        await Promise.resolve(); \
        f(); \
    } catch (e) { \
        save_value(e); \
    } \
})();";

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue value = JS_UNDEFINED;
    JS_SetContextOpaque(ctx, &value);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "save_value", JS_NewCFunction(ctx, save_value, "save_value", 1));
    JS_FreeValue(ctx, global);
    JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    assert(JS_IsJobPending(rt));
    int r = 0;
    while (JS_IsJobPending(rt)) {
        r = JS_ExecutePendingJob(rt, &ctx);
    }
    assert(r == 1);
    assert(!JS_HasException(ctx));
    assert(JS_IsError(ctx, value)); // stack overflow should be caught
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

// https://github.com/quickjs-ng/quickjs/issues/914
static void raw_context_global_var(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContextRaw(rt);
    JS_AddIntrinsicEval(ctx);
    {
        static const char code[] = "globalThis";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "*", JS_EVAL_TYPE_GLOBAL);
        assert(JS_IsException(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        static const char code[] = "var x = 42";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "*", JS_EVAL_TYPE_GLOBAL);
        assert(JS_IsUndefined(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        static const char code[] = "function f() {}";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "*", JS_EVAL_TYPE_GLOBAL);
        assert(JS_IsUndefined(ret));
        JS_FreeValue(ctx, ret);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void is_array(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    {
        static const char code[] = "[]";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "*", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(ret));
        assert(JS_IsArray(ret));
        JS_FreeValue(ctx, ret);
    }
    {
        static const char code[] = "new Proxy([], {})";
        JSValue ret = JS_Eval(ctx, code, strlen(code), "*", JS_EVAL_TYPE_GLOBAL);
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
    JSRuntime *rt = JS_NewRuntime();
    JS_SetDumpFlags(rt, JS_DUMP_MODULE_RESOLVE);
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
    free(buf);
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

int main(void)
{
    sync_call();
    async_call();
    async_call_stack_overflow();
    raw_context_global_var();
    is_array();
    module_serde();
    return 0;
}
