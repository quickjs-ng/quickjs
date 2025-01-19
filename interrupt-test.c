#include <stdlib.h>
#include "quickjs.h"

#define MAX_TIME 10

#define expect(condition)                                       \
    do {                                                        \
        if (!(condition)) {                                     \
            fprintf(stderr, "Failed: %s, file %s, line %d\n",   \
                    #condition, __FILE__, __LINE__);            \
            exit(EXIT_FAILURE);                                 \
        }                                                       \
    } while (0)

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
    expect(time > MAX_TIME);
    expect(JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    expect(JS_HasException(ctx));
    JSValue e = JS_GetException(ctx);
    expect(JS_IsUncatchableError(ctx, e));
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
    expect(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    expect(JS_IsJobPending(rt));
    int r = 0;
    while (JS_IsJobPending(rt)) {
        r = JS_ExecutePendingJob(rt, &ctx);
    }
    expect(time > MAX_TIME);
    expect(r == -1);
    expect(JS_HasException(ctx));
    JSValue e = JS_GetException(ctx);
    expect(JS_IsUncatchableError(ctx, e));
    JS_FreeValue(ctx, e);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue save_value(JSContext *ctx, JSValue this_val, int argc, JSValue *argv)
{
    expect(argc == 1);
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
    JS_SetMaxStackSize(rt, 128 * 1024);
    JS_UpdateStackTop(rt);
    JSValue value = JS_UNDEFINED;
    JS_SetContextOpaque(ctx, &value);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "save_value", JS_NewCFunction(ctx, save_value, "save_value", 1));
    JS_FreeValue(ctx, global);
    JSValue ret = JS_Eval(ctx, code, strlen(code), "<input>", JS_EVAL_TYPE_GLOBAL);
    expect(!JS_IsException(ret));
    JS_FreeValue(ctx, ret);
    expect(JS_IsJobPending(rt));
    int r = 0;
    while (JS_IsJobPending(rt)) {
        r = JS_ExecutePendingJob(rt, &ctx);
    }
    expect(r == 1);
    expect(!JS_HasException(ctx));
    expect(JS_IsError(ctx, value)); /* StackOverflow should be caught */
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main() 
{
    sync_call();
    async_call();
    async_call_stack_overflow();
    printf("interrupt-test passed\n");
    return 0;
}