/**
 * quickjs_coroutine.c
 * QuickJS 协程扩展实现
 */

#include "quickjs_coroutine.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ========== 内部数据结构 ========== */

#define MAX_SESSIONS 65536

struct JSCoroutineManager {
    JSRuntime *rt;

    /* Session 管理 */
    int next_session;

    /* Session -> Waiter 映射表 */
    JSCoroutineWaiter *waiters[MAX_SESSIONS];

    /* 线程安全 */
    pthread_mutex_t lock;

    /* 统计 */
    int waiting_count;
    int total_resumed;
};

/* ========== 辅助函数 ========== */

/* 检查是否是 Generator 对象 */
int JS_IsGenerator(JSContext *ctx, JSValueConst val) {
    if (!JS_IsObject(val)) {
        return 0;
    }

    /* 检查是否有 next 方法 */
    JSValue next = JS_GetPropertyStr(ctx, val, "next");
    int is_func = JS_IsFunction(ctx, next);
    JS_FreeValue(ctx, next);

    if (!is_func) {
        return 0;
    }

    /* 检查是否有 return 和 throw 方法（Generator 特征） */
    JSValue ret = JS_GetPropertyStr(ctx, val, "return");
    JSValue thr = JS_GetPropertyStr(ctx, val, "throw");

    int has_return = JS_IsFunction(ctx, ret);
    int has_throw = JS_IsFunction(ctx, thr);

    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, thr);

    return has_return && has_throw;
}

/* 调用 Generator.next() */
JSValue JS_CallGeneratorNext(JSContext *ctx, JSValue generator, JSValue arg) {
    JSValue next = JS_GetPropertyStr(ctx, generator, "next");
    if (!JS_IsFunction(ctx, next)) {
        JS_FreeValue(ctx, next);
        return JS_ThrowTypeError(ctx, "not a generator");
    }

    JSValue result = JS_Call(ctx, next, generator, 1, &arg);
    JS_FreeValue(ctx, next);

    return result;
}

/* ========== 协程管理器 ========== */

JSCoroutineManager* JS_NewCoroutineManager(JSRuntime *rt) {
    JSCoroutineManager *mgr = calloc(1, sizeof(JSCoroutineManager));
    if (!mgr) return NULL;

    mgr->rt = rt;
    mgr->next_session = 1;
    pthread_mutex_init(&mgr->lock, NULL);

    return mgr;
}

void JS_FreeCoroutineManager(JSCoroutineManager *mgr) {
    if (!mgr) return;

    /* 清理所有等待的 Generator */
    for (int i = 0; i < MAX_SESSIONS; i++) {
        JSCoroutineWaiter *waiter = mgr->waiters[i];
        while (waiter) {
            JSCoroutineWaiter *next = waiter->next;
            JS_FreeValue(waiter->ctx, waiter->generator);
            free(waiter);
            waiter = next;
        }
    }

    pthread_mutex_destroy(&mgr->lock);
    free(mgr);
}

/* ========== Session 管理 ========== */

int JS_CoroutineGenerateSession(JSCoroutineManager *mgr) {
    pthread_mutex_lock(&mgr->lock);

    int session = mgr->next_session++;
    if (mgr->next_session >= MAX_SESSIONS) {
        mgr->next_session = 1;
    }

    pthread_mutex_unlock(&mgr->lock);

    return session;
}

/* ========== Generator 等待与恢复 ========== */

int JS_CoroutineWait(
    JSCoroutineManager *mgr,
    JSContext *ctx,
    JSValue generator,
    int session_id,
    int service_id
) {
    if (!JS_IsGenerator(ctx, generator)) {
        return -1;
    }

    pthread_mutex_lock(&mgr->lock);

    /* 创建等待者 */
    JSCoroutineWaiter *waiter = malloc(sizeof(JSCoroutineWaiter));
    if (!waiter) {
        pthread_mutex_unlock(&mgr->lock);
        return -1;
    }

    waiter->generator = JS_DupValue(ctx, generator);
    waiter->ctx = ctx;
    waiter->session_id = session_id;
    waiter->service_id = service_id;

    /* 添加到等待表 */
    int slot = session_id % MAX_SESSIONS;
    waiter->next = mgr->waiters[slot];
    mgr->waiters[slot] = waiter;

    mgr->waiting_count++;

    pthread_mutex_unlock(&mgr->lock);

    return 0;
}

int JS_CoroutineResume(
    JSCoroutineManager *mgr,
    int session_id,
    JSValue data
) {
    pthread_mutex_lock(&mgr->lock);

    /* 查找等待者 */
    int slot = session_id % MAX_SESSIONS;
    JSCoroutineWaiter *prev = NULL;
    JSCoroutineWaiter *waiter = mgr->waiters[slot];

    while (waiter) {
        if (waiter->session_id == session_id) {
            /* 找到了，从链表中移除 */
            if (prev) {
                prev->next = waiter->next;
            } else {
                mgr->waiters[slot] = waiter->next;
            }
            break;
        }
        prev = waiter;
        waiter = waiter->next;
    }

    pthread_mutex_unlock(&mgr->lock);

    if (!waiter) {
        return -1;  /* 没找到 */
    }

    /* 恢复 Generator */
    JSContext *ctx = waiter->ctx;
    JSValue generator = waiter->generator;

    /* 调用 generator.next(data) */
    JSValue result = JS_CallGeneratorNext(ctx, generator, data);

    if (JS_IsException(result)) {
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, generator);
        free(waiter);
        return -2;
    }

    /* 检查是否完成 */
    JSValue done = JS_GetPropertyStr(ctx, result, "done");
    int is_done = JS_ToBool(ctx, done);
    JS_FreeValue(ctx, done);

    if (!is_done) {
        /* Generator 还在运行，检查是否又 yield 了 */
        JSValue value = JS_GetPropertyStr(ctx, result, "value");

        /* TODO: 处理新的 yield 值 */

        JS_FreeValue(ctx, value);
    }

    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, generator);
    free(waiter);

    pthread_mutex_lock(&mgr->lock);
    mgr->waiting_count--;
    mgr->total_resumed++;
    pthread_mutex_unlock(&mgr->lock);

    return 0;
}

/* ========== JS API 实现 ========== */

/* 全局的协程管理器（临时方案，实际应该绑定到 Runtime） */
static JSCoroutineManager *g_coroutine_mgr = NULL;

/* JS: __coroutine_wait(generator, session, service_id) */
static JSValue js_coroutine_wait(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    if (argc < 3) {
        return JS_ThrowTypeError(ctx, "需要 3 个参数");
    }

    if (!g_coroutine_mgr) {
        return JS_ThrowInternalError(ctx, "协程系统未初始化");
    }

    JSValue generator = argv[0];
    int session, service_id;

    if (JS_ToInt32(ctx, &session, argv[1]) < 0) {
        return JS_EXCEPTION;
    }

    if (JS_ToInt32(ctx, &service_id, argv[2]) < 0) {
        return JS_EXCEPTION;
    }

    int ret = JS_CoroutineWait(g_coroutine_mgr, ctx, generator, session, service_id);

    return JS_NewInt32(ctx, ret);
}

/* JS: __coroutine_resume(session, data) */
JSValue js_coroutine_resume(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "需要 2 个参数");
    }

    if (!g_coroutine_mgr) {
        return JS_ThrowInternalError(ctx, "协程系统未初始化");
    }

    int session;
    if (JS_ToInt32(ctx, &session, argv[0]) < 0) {
        return JS_EXCEPTION;
    }

    int ret = JS_CoroutineResume(g_coroutine_mgr, session, argv[1]);

    return JS_NewInt32(ctx, ret);
}

/* JS: __coroutine_session() */
static JSValue js_coroutine_session(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    if (!g_coroutine_mgr) {
        return JS_ThrowInternalError(ctx, "协程系统未初始化");
    }

    int session = JS_CoroutineGenerateSession(g_coroutine_mgr);

    return JS_NewInt32(ctx, session);
}

/* JS: __is_generator(obj) */
static JSValue js_is_generator(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    if (argc < 1) {
        return JS_FALSE;
    }

    return JS_NewBool(ctx, JS_IsGenerator(ctx, argv[0]));
}

/* ========== 注册 JS API ========== */

int JS_EnableCoroutines(JSContext *ctx, JSCoroutineManager *mgr) {
    /* 暂时使用全局管理器 */
    g_coroutine_mgr = mgr;

    /* 注册全局函数 */
    JSValue global = JS_GetGlobalObject(ctx);

    JS_SetPropertyStr(ctx, global, "__coroutine_wait",
                     JS_NewCFunction(ctx, js_coroutine_wait, "__coroutine_wait", 3));

    JS_SetPropertyStr(ctx, global, "__coroutine_resume",
                     JS_NewCFunction(ctx, js_coroutine_resume, "__coroutine_resume", 2));

    JS_SetPropertyStr(ctx, global, "__coroutine_session",
                     JS_NewCFunction(ctx, js_coroutine_session, "__coroutine_session", 0));

    JS_SetPropertyStr(ctx, global, "__is_generator",
                     JS_NewCFunction(ctx, js_is_generator, "__is_generator", 1));

    JS_FreeValue(ctx, global);

    return 0;
}

/* ========== 服务执行支持 ========== */

JSValue JS_ServiceExecute(JSServiceContext *svc, JSValue input) {
    if (!svc || !JS_IsGenerator(svc->ctx, svc->generator)) {
        return JS_UNDEFINED;
    }

    /* 调用 generator.next(input) */
    JSValue result = JS_CallGeneratorNext(svc->ctx, svc->generator, input);

    if (JS_IsException(result)) {
        return result;
    }

    /* 检查结果 */
    JSValue done = JS_GetPropertyStr(svc->ctx, result, "done");
    int is_done = JS_ToBool(svc->ctx, done);
    JS_FreeValue(svc->ctx, done);

    if (is_done) {
        /* Generator 完成 */
        JSValue value = JS_GetPropertyStr(svc->ctx, result, "value");
        JS_FreeValue(svc->ctx, result);
        return value;
    } else {
        /* Generator yield 了 */
        JSValue value = JS_GetPropertyStr(svc->ctx, result, "value");

        /* 处理 yield 值 */
        JS_HandleYieldValue(svc, value);

        JS_FreeValue(svc->ctx, value);
        JS_FreeValue(svc->ctx, result);

        return JS_UNDEFINED;
    }
}

int JS_HandleYieldValue(JSServiceContext *svc, JSValue yielded) {
    /* 检查是否是 jtask.call */
    JSValue is_call = JS_GetPropertyStr(svc->ctx, yielded, "__jtask_call__");

    if (JS_ToBool(svc->ctx, is_call)) {
        /* 是服务调用 */

        /* 生成 session */
        int session = JS_CoroutineGenerateSession(svc->mgr);

        /* 保存 generator */
        JS_CoroutineWait(svc->mgr, svc->ctx, svc->generator, session, svc->service_id);

        /* TODO: 发送消息到目标服务 */

        JS_FreeValue(svc->ctx, is_call);
        return session;
    }

    JS_FreeValue(svc->ctx, is_call);
    return -1;
}