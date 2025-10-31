/**
 * quickjs_stackful_mini.c
 * QuickJS Stackful Coroutine Implementation (based on minicoro)
 */

#define MINICORO_IMPL
#include "quickjs_stackful_mini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_COROUTINE 16

/* ========== 调度器实现 ========== */

stackful_schedule* stackful_open(JSRuntime *rt, JSContext *main_ctx) {
    stackful_schedule *S = malloc(sizeof(*S));
    S->rt = rt;
    S->main_ctx = main_ctx;
    S->cap = DEFAULT_COROUTINE;
    S->count = 0;
    S->running = -1;
    S->coroutines = calloc(S->cap, sizeof(mco_coro*));
    return S;
}

void stackful_close(stackful_schedule *S) {
    for (int i = 0; i < S->cap; i++) {
        if (S->coroutines[i]) {
            mco_destroy(S->coroutines[i]);
        }
    }
    free(S->coroutines);
    free(S);
}

int stackful_new(stackful_schedule *S, void (*func)(mco_coro*), void *ud) {
    /* 找空位 */
    int id = -1;
    for (int i = 0; i < S->cap; i++) {
        if (S->coroutines[i] == NULL) {
            id = i;
            break;
        }
    }

    if (id == -1) {
        /* 扩容 */
        id = S->cap;
        S->cap *= 2;
        S->coroutines = realloc(S->coroutines, S->cap * sizeof(mco_coro*));
        memset(S->coroutines + id, 0, (S->cap - id) * sizeof(mco_coro*));
    }

    /* 创建协程 */
    mco_desc desc = mco_desc_init(func, 0);
    desc.user_data = ud;

    mco_result res = mco_create(&S->coroutines[id], &desc);
    if (res != MCO_SUCCESS) {
        return -1;
    }

    S->count++;
    return id;
}

int stackful_resume(stackful_schedule *S, int id) {
    if (id < 0 || id >= S->cap || S->coroutines[id] == NULL) {
        return -1;
    }

    // Save the caller's coroutine ID
    int caller_id = S->running;
    
    S->running = id;
    mco_result res = mco_resume(S->coroutines[id]);

    /* 检查状态 */
    mco_state state = mco_status(S->coroutines[id]);
    if (state == MCO_DEAD) {
        /* 协程结束，清理 */
        mco_destroy(S->coroutines[id]);
        S->coroutines[id] = NULL;
        S->count--;
    }

    // Restore the caller's coroutine ID (not -1!)
    S->running = caller_id;
    return res == MCO_SUCCESS ? 0 : -1;
}

void stackful_yield(stackful_schedule *S) {
    int id = S->running;
    if (id < 0) {
        return;
    }

    mco_coro *co = S->coroutines[id];
    if (co) {
        mco_yield(co);
    }
}

mco_state stackful_status(stackful_schedule *S, int id) {
    if (id < 0 || id >= S->cap || S->coroutines[id] == NULL) {
        return MCO_DEAD;
    }
    return mco_status(S->coroutines[id]);
}

int stackful_running(stackful_schedule *S) {
    return S->running;
}

/* ========== QuickJS 集成 ========== */

static stackful_schedule *g_stackful_schedule = NULL;

/* JS: Stackful.yield() */
static JSValue js_stackful_yield(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    if (!g_stackful_schedule) {
        return JS_ThrowInternalError(ctx, "Stackful not initialized");
    }

    stackful_yield(g_stackful_schedule);
    return JS_UNDEFINED;
}

/* JS: Stackful.running() */
static JSValue js_stackful_running(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    if (!g_stackful_schedule) {
        return JS_NewInt32(ctx, -1);
    }

    return JS_NewInt32(ctx, stackful_running(g_stackful_schedule));
}

/* JS: Stackful.status(id) */
static JSValue js_stackful_status(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    if (!g_stackful_schedule) {
        return JS_UNDEFINED;
    }

    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "需要协程 ID");
    }

    int id;
    if (JS_ToInt32(ctx, &id, argv[0]) < 0) {
        return JS_EXCEPTION;
    }

    mco_state state = stackful_status(g_stackful_schedule, id);
    return JS_NewInt32(ctx, state);
}

int stackful_enable_js_api(JSContext *ctx, stackful_schedule *S) {
    g_stackful_schedule = S;

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue stackful_obj = JS_NewObject(ctx);

    /* 注册方法 */
    JS_SetPropertyStr(ctx, stackful_obj, "yield",
                     JS_NewCFunction(ctx, js_stackful_yield, "yield", 0));

    JS_SetPropertyStr(ctx, stackful_obj, "running",
                     JS_NewCFunction(ctx, js_stackful_running, "running", 0));

    JS_SetPropertyStr(ctx, stackful_obj, "status",
                     JS_NewCFunction(ctx, js_stackful_status, "status", 1));

    /* 状态常量 */
    JS_SetPropertyStr(ctx, stackful_obj, "DEAD",
                     JS_NewInt32(ctx, MCO_DEAD));
    JS_SetPropertyStr(ctx, stackful_obj, "SUSPENDED",
                     JS_NewInt32(ctx, MCO_SUSPENDED));
    JS_SetPropertyStr(ctx, stackful_obj, "RUNNING",
                     JS_NewInt32(ctx, MCO_RUNNING));

    JS_SetPropertyStr(ctx, global, "Stackful", stackful_obj);
    JS_FreeValue(ctx, global);

    return 0;
}

stackful_schedule* stackful_get_global_schedule(void) {
    return g_stackful_schedule;
}
