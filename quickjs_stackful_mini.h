/**
 * quickjs_stackful_mini.h
 * QuickJS Stackful Coroutine Support (based on Tina)
 *
 * 真正的 stackful 协程，让普通 JS 函数可以在任何地方 yield
 * 
 * Migration: Minicoro → Tina (2025-11-01)
 * - Higher code quality (9.2/10 vs 7.5/10)
 * - Full test suite
 * - Better documentation
 * - Support for RISC-V and embedded systems
 * - Optional job scheduling system
 */

#ifndef QUICKJS_STACKFUL_MINI_H
#define QUICKJS_STACKFUL_MINI_H

#include "quickjs.h"
#include "../Tina/tina.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Forward declarations ========== */

typedef struct stackful_schedule stackful_schedule;
typedef void (*stackful_func)(void *coro);

/* ========== Extended status enum (Tina only has 'completed') ========== */

typedef enum {
    STACKFUL_STATUS_DEAD = 0,       /* Dead/finished */
    STACKFUL_STATUS_NORMAL = 1,     /* Normal (ready to resume) */
    STACKFUL_STATUS_RUNNING = 2,    /* Currently running */
    STACKFUL_STATUS_SUSPENDED = 3   /* Suspended (yielded) */
} stackful_status_t;

/* ========== Data storage structure ========== */

typedef struct {
    uint8_t buffer[1024];  /* 1KB storage (matches minicoro default) */
    size_t size;           /* Current stored bytes */
} tina_storage;

/* ========== Coroutine wrapper ========== */

typedef struct {
    tina *coro;                  /* Tina coroutine */
    stackful_status_t status;    /* Extended status tracking */
    void *user_data;             /* Original user data */
    stackful_func user_func;     /* Original function */
    int yield_count;             /* Number of yields (for status tracking) */
} tina_wrapper;

/* ========== Scheduler structure ========== */

struct stackful_schedule {
    JSRuntime *rt;
    JSContext *main_ctx;

    /* Coroutine management */
    tina_wrapper **coroutines;   /* Array of tina wrappers */
    tina_storage *storages;      /* Per-coroutine data storage */
    int cap;                     /* Capacity */
    int count;                   /* Active coroutines */

    /* Currently running coroutine */
    int running;                 /* ID of running coroutine (-1 if none) */
};

/* ========== Public API ========== */

/* 创建调度器 */
stackful_schedule* stackful_open(JSRuntime *rt, JSContext *main_ctx);

/* 销毁调度器 */
void stackful_close(stackful_schedule *S);

/* 创建新协程 */
int stackful_new(stackful_schedule *S, stackful_func func, void *ud);

/* 恢复协程 */
int stackful_resume(stackful_schedule *S, int id);

/* 挂起当前协程 */
void stackful_yield(stackful_schedule *S);

/* 挂起当前协程并传递 continue_session 标志 (对齐 ltask 的 coroutine.yield(true)) */
void stackful_yield_with_flag(stackful_schedule *S, int flag);

/* 检查并弹出 continue_session 标志 (对齐 ltask 的 wakeup_session 检测) */
int stackful_pop_continue_flag(stackful_schedule *S, int id);

/* 获取协程状态 */
stackful_status_t stackful_status(stackful_schedule *S, int id);

/* 获取当前运行的协程 ID */
int stackful_running(stackful_schedule *S);

/* ========== QuickJS 集成 ========== */

/* 启用 stackful 支持（注册 JS API） */
int stackful_enable_js_api(JSContext *ctx, stackful_schedule *S);

/* 获取全局调度器 */
stackful_schedule* stackful_get_global_schedule(void);

/* ========== JS API ========== */

/*
 * 在 JS 中可用：
 *
 * Stackful.yield()     - yield 当前协程
 * Stackful.running()   - 获取当前协程 ID
 * Stackful.status(id)  - 获取协程状态
 * 
 * Status constants:
 * Stackful.DEAD        - 0 (已结束)
 * Stackful.NORMAL      - 1 (正常)
 * Stackful.RUNNING     - 2 (运行中)
 * Stackful.SUSPENDED   - 3 (已挂起)
 */

#ifdef __cplusplus
}
#endif

#endif /* QUICKJS_STACKFUL_MINI_H */
