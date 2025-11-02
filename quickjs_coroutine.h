/**
 * quickjs_coroutine.h
 * QuickJS 协程扩展 - 为 JTask 提供协程支持
 *
 * 设计目标：
 * 1. 不修改 quickjs.c 源码
 * 2. 提供 Generator 自动恢复机制
 * 3. 实现 session-based 异步调用
 */

#ifndef QUICKJS_COROUTINE_H
#define QUICKJS_COROUTINE_H

#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 数据结构 ========== */

/* 协程管理器 - 每个 Runtime 一个 */
typedef struct JSCoroutineManager JSCoroutineManager;

/* 协程等待者 */
typedef struct JSCoroutineWaiter {
    JSValue generator;      /* Generator 对象 */
    JSContext *ctx;        /* 所属 Context */
    int session_id;        /* 等待的 session */
    int service_id;        /* 服务 ID */
    struct JSCoroutineWaiter *next;
} JSCoroutineWaiter;

/* ========== 核心 API ========== */

/* 初始化协程系统 */
JSCoroutineManager* JS_NewCoroutineManager(JSRuntime *rt);
void JS_FreeCoroutineManager(JSCoroutineManager *mgr);

/* 为 Context 启用协程支持 */
int JS_EnableCoroutines(JSContext *ctx, JSCoroutineManager *mgr);

/* Session 管理 */
int JS_CoroutineGenerateSession(JSCoroutineManager *mgr);

/* 注册等待的 Generator */
int JS_CoroutineWait(
    JSCoroutineManager *mgr,
    JSContext *ctx,
    JSValue generator,
    int session_id,
    int service_id
);

/* 恢复等待的 Generator */
int JS_CoroutineResume(
    JSCoroutineManager *mgr,
    int session_id,
    JSValue data
);

/* 检查是否是 Generator */
int JS_IsGenerator(JSContext *ctx, JSValueConst val);

/* 调用 Generator.next() */
JSValue JS_CallGeneratorNext(JSContext *ctx, JSValue generator, JSValue arg);

/* ========== JS API ========== */

/* 注册 JS 全局函数 */
int JS_AddCoroutineFunctions(JSContext *ctx);

/* JS 可调用的函数（内部实现） */
JSValue js_coroutine_create(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv);
JSValue js_coroutine_yield(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv);
JSValue js_coroutine_resume(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv);

/* ========== 与 JTask 集成 ========== */

/* 服务执行 Generator */
typedef struct {
    JSContext *ctx;
    JSValue generator;
    int service_id;
    JSCoroutineManager *mgr;
} JSServiceContext;

/* 执行服务 Generator 直到 yield 或完成 */
JSValue JS_ServiceExecute(JSServiceContext *svc, JSValue input);

/* 处理 yield 的值 */
int JS_HandleYieldValue(JSServiceContext *svc, JSValue yielded);

#ifdef __cplusplus
}
#endif

#endif /* QUICKJS_COROUTINE_H */