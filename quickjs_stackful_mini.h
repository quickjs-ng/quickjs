/**
 * quickjs_stackful_mini.h
 * QuickJS Stackful Coroutine Support (based on minicoro)
 *
 * 真正的 stackful 协程，让普通 JS 函数可以在任何地方 yield
 */

#ifndef QUICKJS_STACKFUL_MINI_H
#define QUICKJS_STACKFUL_MINI_H

#include "quickjs.h"
#include "../minicoro/minicoro.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 调度器 ========== */

typedef struct {
    JSRuntime *rt;
    JSContext *main_ctx;

    /* 协程管理 */
    mco_coro **coroutines;
    int cap;
    int count;

    /* 当前运行的协程 */
    int running;
} stackful_schedule;

/* 创建调度器 */
stackful_schedule* stackful_open(JSRuntime *rt, JSContext *main_ctx);

/* 销毁调度器 */
void stackful_close(stackful_schedule *S);

/* 创建新协程 */
int stackful_new(stackful_schedule *S, void (*func)(mco_coro*), void *ud);

/* 恢复协程 */
int stackful_resume(stackful_schedule *S, int id);

/* 挂起当前协程 */
void stackful_yield(stackful_schedule *S);

/* 获取协程状态 */
mco_state stackful_status(stackful_schedule *S, int id);

/* 获取当前运行的协程 ID */
int stackful_running(stackful_schedule *S);

/* ========== Status constants (from minicoro) ========== */

#define STACKFUL_STATUS_DEAD MCO_DEAD           /* 0 = Dead/finished */
#define STACKFUL_STATUS_NORMAL MCO_NORMAL       /* 1 = Normal */
#define STACKFUL_STATUS_RUNNING MCO_RUNNING     /* 2 = Running */
#define STACKFUL_STATUS_SUSPENDED MCO_SUSPENDED /* 3 = Suspended */

/* ========== QuickJS 集成 ========== */

/* 启用 stackful 支持（注册 JS API） */
int stackful_enable_js_api(JSContext *ctx, stackful_schedule *S);

/* 获取全局调度器 */
stackful_schedule* stackful_get_global_schedule(void);

/* ========== JS API ========== */

/*
 * 在 JS 中可用：
 *
 * Stackful.yield()  - yield 当前协程
 * Stackful.running() - 获取当前协程 ID
 * Stackful.status(id) - 获取协程状态
 */

#ifdef __cplusplus
}
#endif

#endif /* QUICKJS_STACKFUL_MINI_H */
