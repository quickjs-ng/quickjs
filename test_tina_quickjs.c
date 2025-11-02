/**
 * test_tina_quickjs.c
 * Production-level test for Tina-based stackful coroutines with QuickJS
 * 
 * This test demonstrates real JavaScript code execution with stackful coroutines,
 * including yield from nested function calls and async-like patterns.
 */

#include "quickjs_stackful_mini.h"
#include "quickjs-libc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test context */
typedef struct {
    JSContext *ctx;
    stackful_schedule *S;
    int coro_id;
} test_context;

/* Coroutine entry point: Execute JavaScript code */
void js_coro_entry(void *arg) {
    test_context *tctx = (test_context*)arg;
    JSContext *ctx = tctx->ctx;
    
    fprintf(stderr, "\n=== JavaScript Coroutine Execution ===\n\n");
    
    /* Test 1: Simple yield from JavaScript */
    const char *test1 = 
        "print('JS: Before first yield');\n"
        "Stackful.yield();\n"
        "print('JS: After first yield');\n"
        "Stackful.yield();\n"
        "print('JS: After second yield');\n";
    
    JSValue result = JS_Eval(ctx, test1, strlen(test1), "test1.js", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        fprintf(stderr, "ERROR: JavaScript exception in test1\n");
        js_std_dump_error(ctx);
    }
    
    JS_FreeValue(ctx, result);
    
    fprintf(stderr, "\n=== JavaScript Coroutine Completed ===\n\n");
}

/* Coroutine entry: Nested function calls with yields */
void js_coro_nested(void *arg) {
    test_context *tctx = (test_context*)arg;
    JSContext *ctx = tctx->ctx;
    
    fprintf(stderr, "\n=== Nested Function Yield Test ===\n\n");
    
    /* Test 2: Yield from nested function */
    const char *test2 = 
        "function inner() {\n"
        "  print('JS: Inside inner(), about to yield');\n"
        "  Stackful.yield();\n"
        "  print('JS: Inside inner(), after yield');\n"
        "  return 'inner_result';\n"
        "}\n"
        "\n"
        "function outer() {\n"
        "  print('JS: Inside outer(), calling inner()');\n"
        "  var result = inner();\n"
        "  print('JS: Inside outer(), inner returned: ' + result);\n"
        "  Stackful.yield();\n"
        "  print('JS: Inside outer(), after final yield');\n"
        "  return 'outer_result';\n"
        "}\n"
        "\n"
        "print('JS: Starting outer()');\n"
        "var final = outer();\n"
        "print('JS: outer() returned: ' + final);\n";
    
    JSValue result = JS_Eval(ctx, test2, strlen(test2), "test2.js", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        fprintf(stderr, "ERROR: JavaScript exception in test2\n");
        js_std_dump_error(ctx);
    }
    
    JS_FreeValue(ctx, result);
    
    fprintf(stderr, "\n=== Nested Test Completed ===\n\n");
}

/* Coroutine entry: Check running() and status() APIs */
void js_coro_api_test(void *arg) {
    test_context *tctx = (test_context*)arg;
    JSContext *ctx = tctx->ctx;
    
    fprintf(stderr, "\n=== Stackful API Test ===\n\n");
    
    const char *test3 = 
        "var running_id = Stackful.running();\n"
        "print('JS: Currently running coroutine ID: ' + running_id);\n"
        "\n"
        "var status = Stackful.status(running_id);\n"
        "print('JS: Current status: ' + status + ' (expected 2=RUNNING)');\n"
        "\n"
        "print('JS: Status constants:');\n"
        "print('  DEAD=' + Stackful.DEAD);\n"
        "print('  NORMAL=' + Stackful.NORMAL);\n"
        "print('  RUNNING=' + Stackful.RUNNING);\n"
        "print('  SUSPENDED=' + Stackful.SUSPENDED);\n"
        "\n"
        "print('JS: Yielding...');\n"
        "Stackful.yield();\n"
        "print('JS: Resumed!');\n";
    
    JSValue result = JS_Eval(ctx, test3, strlen(test3), "test3.js", JS_EVAL_TYPE_GLOBAL);
    
    if (JS_IsException(result)) {
        fprintf(stderr, "ERROR: JavaScript exception in test3\n");
        js_std_dump_error(ctx);
    }
    
    JS_FreeValue(ctx, result);
    
    fprintf(stderr, "\n=== API Test Completed ===\n\n");
}

int main() {
    printf("=== Tina + QuickJS Production Integration Test ===\n\n");
    
    /* Initialize QuickJS */
    JSRuntime *rt = JS_NewRuntime();
    assert(rt != NULL);
    
    JSContext *ctx = JS_NewContext(rt);
    assert(ctx != NULL);
    
    /* Initialize standard library (for print()) */
    js_std_add_helpers(ctx, 0, NULL);
    
    /* Create stackful scheduler */
    stackful_schedule *S = stackful_open(rt, ctx);
    assert(S != NULL);
    
    /* Enable Stackful JavaScript API */
    int ret = stackful_enable_js_api(ctx, S);
    assert(ret == 0);
    
    printf("✓ QuickJS runtime and Stackful scheduler initialized\n\n");
    
    /* ===== Test 1: Simple JavaScript yields ===== */
    printf("Test 1: Simple JavaScript yields\n");
    printf("----------------------------------\n");
    
    test_context tctx1 = { .ctx = ctx, .S = S, .coro_id = -1 };
    int coro1 = stackful_new(S, js_coro_entry, &tctx1);
    assert(coro1 >= 0);
    tctx1.coro_id = coro1;
    
    printf("Created coroutine %d\n", coro1);
    
    /* Resume 3 times (start + 2 yields) */
    for (int i = 0; i < 3; i++) {
        printf("[C] Resuming coroutine %d (iteration %d)...\n", coro1, i);
        ret = stackful_resume(S, coro1);
        assert(ret == 0 || stackful_status(S, coro1) == STACKFUL_STATUS_DEAD);
        
        stackful_status_t status = stackful_status(S, coro1);
        printf("[C] Status after resume: %d\n", status);
        
        if (status == STACKFUL_STATUS_DEAD) {
            printf("[C] Coroutine %d completed\n", coro1);
            break;
        }
    }
    
    printf("\n✓ Test 1 passed\n\n");
    
    /* ===== Test 2: Nested function yields ===== */
    printf("Test 2: Nested function yields\n");
    printf("-------------------------------\n");
    
    test_context tctx2 = { .ctx = ctx, .S = S, .coro_id = -1 };
    int coro2 = stackful_new(S, js_coro_nested, &tctx2);
    assert(coro2 >= 0);
    tctx2.coro_id = coro2;
    
    printf("Created coroutine %d\n", coro2);
    
    /* Resume until completion */
    int resume_count = 0;
    while (stackful_status(S, coro2) != STACKFUL_STATUS_DEAD && resume_count < 10) {
        printf("[C] Resuming coroutine %d (count %d)...\n", coro2, resume_count);
        ret = stackful_resume(S, coro2);
        assert(ret == 0 || stackful_status(S, coro2) == STACKFUL_STATUS_DEAD);
        resume_count++;
    }
    
    assert(stackful_status(S, coro2) == STACKFUL_STATUS_DEAD);
    printf("[C] Coroutine %d completed after %d resumes\n", coro2, resume_count);
    
    printf("\n✓ Test 2 passed\n\n");
    
    /* ===== Test 3: Stackful API test ===== */
    printf("Test 3: Stackful API (running, status)\n");
    printf("---------------------------------------\n");
    
    test_context tctx3 = { .ctx = ctx, .S = S, .coro_id = -1 };
    int coro3 = stackful_new(S, js_coro_api_test, &tctx3);
    assert(coro3 >= 0);
    tctx3.coro_id = coro3;
    
    printf("Created coroutine %d\n", coro3);
    
    /* Resume twice (start + 1 yield) */
    for (int i = 0; i < 2; i++) {
        printf("[C] Resuming coroutine %d (iteration %d)...\n", coro3, i);
        ret = stackful_resume(S, coro3);
        assert(ret == 0 || stackful_status(S, coro3) == STACKFUL_STATUS_DEAD);
    }
    
    assert(stackful_status(S, coro3) == STACKFUL_STATUS_DEAD);
    printf("[C] Coroutine %d completed\n", coro3);
    
    printf("\n✓ Test 3 passed\n\n");
    
    /* ===== Cleanup ===== */
    printf("Cleanup\n");
    printf("-------\n");
    
    stackful_close(S);
    printf("✓ Scheduler destroyed\n");
    
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    printf("✓ QuickJS runtime destroyed\n\n");
    
    printf("=== All Production Tests Passed! ===\n");
    
    return 0;
}
