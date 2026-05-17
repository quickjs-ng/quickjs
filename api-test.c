#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
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

static void property_str_typed_accessors(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue r = eval(ctx, "({a: 42, b: 'hi', c: true, d: 3.5, e: null, f: 'not a number'})");
    assert(!JS_IsException(r));

    /* int32 */
    assert(JS_GetPropertyStrInt32(ctx, r, "a", -1) == 42);
    assert(JS_GetPropertyStrInt32(ctx, r, "missing", 7) == 7);
    assert(JS_GetPropertyStrInt32(ctx, r, "e", 7) == 7); /* null -> fallback */
    /* string coerces to int via ToInt32: "not a number" -> NaN -> 0 */
    assert(JS_GetPropertyStrInt32(ctx, r, "f", -1) == 0);
    assert(!JS_HasException(ctx));

    /* int64 */
    assert(JS_GetPropertyStrInt64(ctx, r, "a", -1) == 42);
    assert(JS_GetPropertyStrInt64(ctx, r, "missing", 99) == 99);

    /* float64 */
    assert(JS_GetPropertyStrFloat64(ctx, r, "d", 0.0) == 3.5);
    assert(JS_GetPropertyStrFloat64(ctx, r, "missing", 1.25) == 1.25);

    /* bool */
    assert(JS_GetPropertyStrBool(ctx, r, "c", -1) == 1);
    assert(JS_GetPropertyStrBool(ctx, r, "a", -1) == 1); /* 42 is truthy */
    assert(JS_GetPropertyStrBool(ctx, r, "missing", -1) == -1);
    assert(JS_GetPropertyStrBool(ctx, r, "e", -1) == -1); /* null -> fallback */

    /* exception in getter is swallowed, returns fallback */
    JS_FreeValue(ctx, r);
    r = eval(ctx, "({get x(){ throw new Error('boom'); }})");
    assert(!JS_IsException(r));
    assert(JS_GetPropertyStrInt32(ctx, r, "x", 11) == 11);
    assert(!JS_HasException(ctx));

    JS_FreeValue(ctx, r);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void free_cstring_safe(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    /* no-op on NULL should not crash */
    JS_FreeCStringSafe(ctx, NULL);
    JSValue v = JS_NewString(ctx, "hello");
    const char *s = JS_ToCString(ctx, v);
    JS_FreeValue(ctx, v);
    JS_FreeCStringSafe(ctx, s);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void exception_as_string(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    /* no pending exception -> NULL */
    const char *s = JS_GetExceptionAsString(ctx, true);
    assert(s == NULL);

    /* throw a string (not an Error) -> just the message */
    JSValue r = eval(ctx, "throw 'oops'");
    assert(JS_IsException(r));
    JS_FreeValue(ctx, r);
    s = JS_GetExceptionAsString(ctx, true);
    assert(s != NULL);
    assert(strcmp(s, "oops") == 0);
    JS_FreeCString(ctx, s);
    assert(!JS_HasException(ctx));

    /* throw an Error, ask for stack */
    r = eval(ctx, "throw new Error('boom')");
    assert(JS_IsException(r));
    JS_FreeValue(ctx, r);
    s = JS_GetExceptionAsString(ctx, true);
    assert(s != NULL);
    assert(strstr(s, "Error: boom") != NULL);
    assert(strchr(s, '\n') != NULL); /* has a stack line */
    JS_FreeCString(ctx, s);

    /* throw an Error, no stack requested */
    r = eval(ctx, "throw new TypeError('nope')");
    assert(JS_IsException(r));
    JS_FreeValue(ctx, r);
    s = JS_GetExceptionAsString(ctx, false);
    assert(s != NULL);
    assert(strstr(s, "nope") != NULL);
    JS_FreeCString(ctx, s);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void resolve_reject_promise(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global = JS_GetGlobalObject(ctx);

    /* resolve case */
    JSValue resolvers[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolvers);
    JS_SetPropertyStr(ctx, global, "p1", JS_DupValue(ctx, promise));
    JS_FreeValue(ctx, promise);
    JS_ResolvePromise(ctx, resolvers, JS_NewInt32(ctx, 7));

    JSValue r = eval(ctx,
        "let v1; p1.then(x => { v1 = x; });");
    JS_FreeValue(ctx, r);
    JSContext *out = NULL;
    while (JS_ExecutePendingJob(rt, &out) > 0) ;
    r = eval(ctx, "v1");
    int32_t got = -1;
    JS_ToInt32(ctx, &got, r);
    assert(got == 7);
    JS_FreeValue(ctx, r);

    /* reject case */
    promise = JS_NewPromiseCapability(ctx, resolvers);
    JS_SetPropertyStr(ctx, global, "p2", JS_DupValue(ctx, promise));
    JS_FreeValue(ctx, promise);
    JS_RejectPromise(ctx, resolvers, JS_NewString(ctx, "fail"));

    r = eval(ctx,
        "let v2; p2.catch(e => { v2 = e; });");
    JS_FreeValue(ctx, r);
    while (JS_ExecutePendingJob(rt, &out) > 0) ;
    r = eval(ctx, "v2");
    const char *got_s = JS_ToCString(ctx, r);
    assert(got_s && strcmp(got_s, "fail") == 0);
    JS_FreeCString(ctx, got_s);
    JS_FreeValue(ctx, r);

    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void property_str_string_accessor(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue r = eval(ctx, "({a: 'hello', b: 42, c: null, d: undefined})");
    assert(!JS_IsException(r));

    const char *s = JS_GetPropertyStrString(ctx, r, "a");
    assert(s && strcmp(s, "hello") == 0);
    JS_FreeCString(ctx, s);

    /* number coerces to its string form */
    s = JS_GetPropertyStrString(ctx, r, "b");
    assert(s && strcmp(s, "42") == 0);
    JS_FreeCString(ctx, s);

    /* null/undefined/missing => NULL */
    assert(JS_GetPropertyStrString(ctx, r, "c") == NULL);
    assert(JS_GetPropertyStrString(ctx, r, "d") == NULL);
    assert(JS_GetPropertyStrString(ctx, r, "missing") == NULL);
    assert(!JS_HasException(ctx));

    /* JS_FreeCStringSafe accepts NULL */
    JS_FreeCStringSafe(ctx, JS_GetPropertyStrString(ctx, r, "missing"));

    /* getter that throws => NULL, no exception left pending */
    JS_FreeValue(ctx, r);
    r = eval(ctx, "({get x(){ throw new Error('boom'); }})");
    assert(!JS_IsException(r));
    assert(JS_GetPropertyStrString(ctx, r, "x") == NULL);
    assert(!JS_HasException(ctx));

    JS_FreeValue(ctx, r);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void property_str_typed_setters(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue obj = JS_NewObject(ctx);
    assert(JS_SetPropertyStrInt32(ctx, obj, "i32", -7) >= 0);
    assert(JS_SetPropertyStrInt64(ctx, obj, "i64",
                                  (int64_t)1 << 40) >= 0);
    assert(JS_SetPropertyStrFloat64(ctx, obj, "f64", 1.5) >= 0);
    assert(JS_SetPropertyStrBool(ctx, obj, "t", 1) >= 0);
    assert(JS_SetPropertyStrBool(ctx, obj, "f", 0) >= 0);
    assert(JS_SetPropertyStrString(ctx, obj, "s", "ok") >= 0);

    assert(JS_GetPropertyStrInt32(ctx, obj, "i32", 0) == -7);
    assert(JS_GetPropertyStrInt64(ctx, obj, "i64", 0) == ((int64_t)1 << 40));
    assert(JS_GetPropertyStrFloat64(ctx, obj, "f64", 0.0) == 1.5);
    assert(JS_GetPropertyStrBool(ctx, obj, "t", -1) == 1);
    assert(JS_GetPropertyStrBool(ctx, obj, "f", -1) == 0);
    const char *s = JS_GetPropertyStrString(ctx, obj, "s");
    assert(s && strcmp(s, "ok") == 0);
    JS_FreeCString(ctx, s);

    /* round-trip through a JS expression to verify each stored value
       has the JS type its setter advertises */
    JSValue g = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, g, "_o", JS_DupValue(ctx, obj));
    JSValue r = eval(ctx, "typeof _o.i32 + ',' + typeof _o.s + ',' + typeof _o.t");
    const char *types = JS_ToCString(ctx, r);
    assert(types && strcmp(types, "number,string,boolean") == 0);
    JS_FreeCString(ctx, types);
    JS_FreeValue(ctx, r);
    JS_FreeValue(ctx, g);

    JS_FreeValue(ctx, obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue cb_arg_helpers(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    if (JS_CheckArgc(ctx, argc, 2, 4) < 0)
        return JS_EXCEPTION;
    int32_t i;
    double d;
    const char *s = NULL;
    if (JS_ArgInt32(ctx, argc, argv, 0, &i) < 0)
        return JS_EXCEPTION;
    if (JS_ArgFloat64(ctx, argc, argv, 1, &d) < 0)
        return JS_EXCEPTION;
    if (argc >= 3 && JS_ArgCString(ctx, argc, argv, 2, &s) < 0)
        return JS_EXCEPTION;
    int b = 0;
    if (argc >= 4 && JS_ArgBool(ctx, argc, argv, 3, &b) < 0) {
        JS_FreeCStringSafe(ctx, s);
        return JS_EXCEPTION;
    }
    /* assemble a result string for easy inspection from JS */
    char buf[128];
    snprintf(buf, sizeof buf, "%d|%g|%s|%d", (int)i, d, s ? s : "-", b);
    JS_FreeCStringSafe(ctx, s);
    return JS_NewString(ctx, buf);
}

static void arg_helpers(void)
{
    JSRuntime *rt = new_runtime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue g = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, g, "f",
                      JS_NewCFunction(ctx, cb_arg_helpers, "f", 4));
    JS_FreeValue(ctx, g);

    /* happy path: required pair */
    JSValue r = eval(ctx, "f(3, 1.5)");
    const char *s = JS_ToCString(ctx, r);
    assert(s && strcmp(s, "3|1.5|-|0") == 0);
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, r);

    /* all four optional args supplied */
    r = eval(ctx, "f(10, 2.5, 'hi', 1)");
    s = JS_ToCString(ctx, r);
    assert(s && strcmp(s, "10|2.5|hi|1") == 0);
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, r);

    /* too few -> TypeError from JS_CheckArgc */
    r = eval(ctx, "try{ f(1); 'no-throw' }catch(e){ String(e) }");
    s = JS_ToCString(ctx, r);
    assert(s && strstr(s, "TypeError") && strstr(s, "at least 2"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, r);

    /* too many -> TypeError */
    r = eval(ctx, "try{ f(1,2,3,4,5); 'no-throw' }catch(e){ String(e) }");
    s = JS_ToCString(ctx, r);
    assert(s && strstr(s, "TypeError") && strstr(s, "at most 4"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, r);

    /* conversion error on argv[0] (Symbol -> Int32) propagates as a thrown
       TypeError from JS_ToInt32 */
    r = eval(ctx, "try{ f(Symbol(), 1); 'no-throw' }catch(e){ String(e) }");
    s = JS_ToCString(ctx, r);
    assert(s && strstr(s, "TypeError"));
    JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, r);

    /* unbounded max: pass max=-1 */
    assert(JS_CheckArgc(ctx, /*argc*/ 5, /*min*/ 1, /*max*/ -1) == 0);
    /* exact match */
    assert(JS_CheckArgc(ctx, 2, 2, 2) == 0);
    /* below min -> throws */
    assert(JS_CheckArgc(ctx, 0, 1, 3) == -1);
    assert(JS_HasException(ctx));
    JS_FreeValue(ctx, JS_GetException(ctx));

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
    runtime_cstring_free();
    utf16_string();
    weak_map_gc_check();
    promise_hook();
    dump_memory_usage();
    new_errors();
    global_object_prototype();
    slice_string_tocstring();
    immutable_array_buffer();
    get_uint8array();
    new_symbol();
    property_str_typed_accessors();
    property_str_string_accessor();
    property_str_typed_setters();
    arg_helpers();
    free_cstring_safe();
    exception_as_string();
    resolve_reject_promise();
    return 0;
}
