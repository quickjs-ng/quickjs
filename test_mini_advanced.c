/**
 * test_mini_advanced.c
 * 高级测试：循环、嵌套、匿名函数里 yield
 */

#include "quickjs.h"
#include "quickjs-libc.h"
#include "quickjs_stackful_mini.h"
#include <stdio.h>
#include <string.h>

static int call_count = 0;

/* 模拟 jtask.call - 会 yield 的 C 函数 */
static JSValue js_mock_call(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    call_count++;

    const char *name = "unknown";
    if (argc > 0) {
        name = JS_ToCString(ctx, argv[0]);
    }

    printf("[mock_call #%d] '%s' 调用，yielding...\n", call_count, name);

    /* Yield 协程 */
    stackful_schedule *S = stackful_get_global_schedule();
    if (S) {
        stackful_yield(S);
    }

    printf("[mock_call #%d] '%s' resumed\n", call_count, name);

    if (argc > 0) {
        JS_FreeCString(ctx, name);
    }

    return JS_NewInt32(ctx, call_count);
}

/* 测试代码 */
const char *test_code =
"console.log('=== Test 1: For 循环 ===');\n"
"for (let i = 0; i < 3; i++) {\n"
"    console.log('Loop iteration:', i);\n"
"    const result = mock_call('for-loop');\n"  // <- 循环里调用会 yield 的函数
"    console.log('Got result:', result);\n"
"}\n"
"\n"
"console.log('\\n=== Test 2: While 循环 ===');\n"
"let count = 0;\n"
"while (count < 2) {\n"
"    console.log('While count:', count);\n"
"    const result = mock_call('while-loop');\n"  // <- while 里 yield
"    console.log('Got result:', result);\n"
"    count++;\n"
"}\n"
"\n"
"console.log('\\n=== Test 3: 嵌套函数 ===');\n"
"function outer() {\n"
"    console.log('Outer start');\n"
"    \n"
"    function inner() {\n"
"        console.log('Inner calling mock_call');\n"
"        return mock_call('nested');  // <- 嵌套函数里 yield\n"
"    }\n"
"    \n"
"    const result = inner();\n"
"    console.log('Outer got:', result);\n"
"    return result;\n"
"}\n"
"outer();\n"
"\n"
"console.log('\\n=== Test 4: 匿名函数 ===');\n"
"const anonymous = function() {\n"
"    console.log('Anonymous function');\n"
"    return mock_call('anonymous');  // <- 匿名函数里 yield\n"
"};\n"
"const r = anonymous();\n"
"console.log('Anonymous result:', r);\n"
"\n"
"console.log('\\n=== Test 5: 箭头函数 ===');\n"
"const arrow = () => {\n"
"    console.log('Arrow function');\n"
"    return mock_call('arrow');  // <- 箭头函数里 yield\n"
"};\n"
"const ar = arrow();\n"
"console.log('Arrow result:', ar);\n"
"\n"
"console.log('\\n=== Test 6: 回调里 yield ===');\n"
"[1, 2].forEach((item) => {\n"
"    console.log('ForEach item:', item);\n"
"    const result = mock_call('callback');  // <- 回调里 yield\n"
"    console.log('Callback result:', result);\n"
"});\n"
"\n"
"console.log('\\n=== All tests passed! ===');\n";

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

/* 协程入口 */
void js_coro_entry(mco_coro* co) {
    JSContext *ctx = (JSContext *)mco_get_user_data(co);

    printf("\n=== 开始执行高级测试 ===\n\n");

    JSValue result = JS_Eval(ctx, test_code, strlen(test_code),
                             "test.js", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        printf("\n错误:\n");
        JSValue exception = JS_GetException(ctx);
        const char *str = JS_ToCString(ctx, exception);
        if (str) {
            printf("%s\n", str);
            JS_FreeCString(ctx, str);
        }
        JS_FreeValue(ctx, exception);
    }

    JS_FreeValue(ctx, result);
}

int main() {
    printf("=== QuickJS Stackful Advanced Test ===\n");
    printf("测试：循环、嵌套、匿名函数、箭头函数、回调\n");

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
                     JS_NewCFunction(ctx, js_mock_call, "mock_call", 1));

    JS_FreeValue(ctx, global);

    /* 创建 stackful 调度器 */
    stackful_schedule *S = stackful_open(rt, ctx);
    stackful_enable_js_api(ctx, S);

    /* 创建协程 */
    int coro_id = stackful_new(S, js_coro_entry, ctx);

    /* 运行：需要多次 resume */
    printf("\n>>> 开始运行协程（自动 resume 直到结束）\n\n");

    int resume_count = 0;
    while (stackful_status(S, coro_id) != MCO_DEAD) {
        stackful_resume(S, coro_id);
        resume_count++;

        if (stackful_status(S, coro_id) == MCO_SUSPENDED) {
            // 协程挂起了，继续 resume
        }

        if (resume_count > 100) {
            printf("ERROR: Too many resumes!\n");
            break;
        }
    }

    printf("\n>>> 协程执行完毕\n");
    printf("总共 resume 了 %d 次\n", resume_count);
    printf("总共调用了 %d 次 mock_call\n", call_count);

    /* 清理 */
    stackful_close(S);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    printf("\n=== 测试完成 ===\n");
    printf(call_count >= 10 ? "✅ PASS\n" : "❌ FAIL\n");

    return call_count >= 10 ? 0 : 1;
}
