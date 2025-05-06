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
    static const char code[] =
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
    static const char code[] =
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

static void two_byte_string(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    {
        JSValue v = JS_NewTwoByteString(ctx, NULL, 0);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        assert(!strcmp(s, ""));
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewTwoByteString(ctx, (uint16_t[]){'o','k'}, 2);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        assert(!strcmp(s, "ok"));
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, v);
    }
    {
        JSValue v = JS_NewTwoByteString(ctx, (uint16_t[]){0xD800}, 1);
        assert(!JS_IsException(v));
        const char *s = JS_ToCString(ctx, v);
        assert(s);
        // questionable but surrogates don't map to UTF-8 without WTF-8
        assert(!strcmp(s, "\xED\xA0\x80"));
        JS_FreeCString(ctx, s);
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

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    JSValue ret = JS_Eval(ctx, init_code, strlen(init_code), "<input>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(ret));

    JSValue ret_test = JS_Eval(ctx, test_code, strlen(test_code), "<input>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(ret_test));
    JS_RunGC(rt);
    JSMemoryUsage memory_usage;
    JS_ComputeMemoryUsage(rt, &memory_usage);

    for (int i = 0; i < 3; i++) {
        JSValue ret_test2 = JS_Eval(ctx, test_code, strlen(test_code), "<input>", JS_EVAL_TYPE_GLOBAL);
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
    JSRuntime *rt = JS_NewRuntime();
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

    rt = JS_NewRuntime();
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

int main(void)
{
    sync_call();
    async_call();
    async_call_stack_overflow();
    raw_context_global_var();
    is_array();
    module_serde();
    two_byte_string();
    weak_map_gc_check();
    promise_hook();
    dump_memory_usage();
    return 0;
}
