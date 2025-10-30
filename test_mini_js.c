/**
 * test_mini_js.c
 * 测试 QuickJS + minicoro stackful 协程
 * 目标：让普通 JS 函数可以 yield
 */

#include "quickjs.h"
#include "quickjs-libc.h"
#include "quickjs_stackful_mini.h"
#include <stdio.h>
#include <string.h>

/* 模拟 jtask.call - 会 yield 的 C 函数 */
static JSValue js_mock_call(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    printf("[mock_call] 被调用\n");

    /* Yield 协程 */
    stackful_schedule *S = stackful_get_global_schedule();
    if (S) {
        printf("[mock_call] Yielding...\n");
        stackful_yield(S);
        printf("[mock_call] Resumed!\n");
    }

    return JS_NewString(ctx, "result from mock_call");
}

/* 测试 JS 代码 */
const char *test_code =
"// 这是普通函数，不是 generator！\n"
"function testNormalFunction() {\n"
"    console.log('函数开始');\n"
"    \n"
"    // 调用会 yield 的函数\n"
"    const result = mock_call();\n"
"    \n"
"    console.log('收到结果:', result);\n"
"    return result;\n"
"}\n"
"\n"
"testNormalFunction();\n";

static JSValue js_print(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        if (i > 0) printf(" ");
        const char *str = JS_ToCString(ctx, argv[i]);
        if (str) {
            printf("%s", str);
            JS_FreeCString(ctx, str);
        }
    }
    printf("\n");
    return JS_UNDEFINED;
}

/* 协程入口 - 执行 JS 代码 */
void js_coro_entry(mco_coro* co) {
    JSContext *ctx = (JSContext *)mco_get_user_data(co);

    printf("\n=== 协程开始执行 JS ===\n\n");

    JSValue result = JS_Eval(ctx, test_code, strlen(test_code),
                             "test.js", JS_EVAL_TYPE_GLOBAL);

    printf("\n[DEBUG] JS_Eval 返回\n");

    if (JS_IsException(result)) {
        printf("\n错误:\n");
        JSValue exception = JS_GetException(ctx);
        const char *str = JS_ToCString(ctx, exception);
        if (str) {
            printf("%s\n", str);
            JS_FreeCString(ctx, str);
        }
        JS_FreeValue(ctx, exception);
    } else {
        printf("\n执行成功\n");
    }

    JS_FreeValue(ctx, result);

    printf("\n[DEBUG] 协程即将结束\n");
}

int main() {
    printf("=== QuickJS + Minicoro Stackful Test ===\n");

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    /* 添加 console.log */
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log",
                     JS_NewCFunction(ctx, js_print, "log", 1));
    JS_SetPropertyStr(ctx, global, "console", console);

    /* 添加 mock_call */
    JS_SetPropertyStr(ctx, global, "mock_call",
                     JS_NewCFunction(ctx, js_mock_call, "mock_call", 0));

    JS_FreeValue(ctx, global);

    /* 创建 stackful 调度器 */
    stackful_schedule *S = stackful_open(rt, ctx);
    stackful_enable_js_api(ctx, S);

    /* 创建协程执行 JS */
    int coro_id = stackful_new(S, js_coro_entry, ctx);
    printf("\n创建协程 ID: %d\n", coro_id);

    /* 首次运行 */
    printf("\n>>> Resume 1 (启动协程)\n");
    stackful_resume(S, coro_id);

    printf("\n>>> 协程 yield 了\n");
    printf("协程状态: %d\n", stackful_status(S, coro_id));

    /* 第二次恢复 */
    printf("\n>>> Resume 2 (恢复协程)\n");
    stackful_resume(S, coro_id);

    printf("\n>>> 协程执行完毕\n");
    printf("协程状态: %d (0=DEAD)\n", stackful_status(S, coro_id));

    /* 清理 */
    stackful_close(S);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    printf("\n=== 测试完成 ===\n");
    return 0;
}
