/**
 * quickjs_stackful.h
 * QuickJS Stackful Coroutine Support
 *
 * 真正的 stackful 协程，让普通 JS 函数可以在任何地方 yield
 * 基于 ucontext 实现独立栈切换
 */

#ifndef QUICKJS_STACKFUL_H
#define QUICKJS_STACKFUL_H

#include "quickjs.h"

#ifdef __APPLE__
    #include <sys/ucontext.h>
#else
    #include <ucontext.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 配置常量 ========== */

#define STACKFUL_STACK_SIZE (1024 * 1024)  /* 每个协程 1MB 栈 */
#define STACKFUL_MAX_COROUTINES 1024       /* 最大协程数 */

/* ========== 协程状态 ========== */

typedef enum {
    STACKFUL_DEAD = 0,      /* 已结束 */
    STACKFUL_READY = 1,     /* 已创建，未运行 */
    STACKFUL_RUNNING = 2,   /* 正在运行 */
    STACKFUL_SUSPENDED = 3  /* 已挂起 */
} StackfulStatus;

/* ========== 核心数据结构 ========== */

/* 协程 */
struct stackful_coroutine;

/* 调度器 */
typedef struct {
    char stack[STACKFUL_STACK_SIZE];  /* 共享栈（切换时临时用） */
    ucontext_t main;                  /* 主上下文 */
    int nco;                          /* 活跃协程数 */
    int cap;                          /* 容量 */
    int running;                      /* 当前运行的协程 ID */
    struct stackful_coroutine **co;   /* 协程数组 */

    /* QuickJS 关联 */
    JSRuntime *rt;
    JSContext *main_ctx;              /* 主 Context */
} stackful_schedule;

/* 协程执行函数 */
typedef void (*stackful_func)(stackful_schedule *S, void *ud);

/* ========== 协程管理 API（C 层）========== */

/* 创建调度器 */
stackful_schedule* stackful_open(JSRuntime *rt, JSContext *main_ctx);

/* 销毁调度器 */
void stackful_close(stackful_schedule *S);

/* 创建新协程 */
int stackful_new(stackful_schedule *S, stackful_func func, void *ud);

/* 恢复协程 */
void stackful_resume(stackful_schedule *S, int id);

/* 挂起当前协程 */
void stackful_yield(stackful_schedule *S);

/* 获取协程状态 */
int stackful_status(stackful_schedule *S, int id);

/* 获取当前运行的协程 ID */
int stackful_running(stackful_schedule *S);

/* ========== QuickJS 集成 API ========== */

/* JS 函数执行上下文 */
typedef struct {
    JSContext *ctx;           /* 专属 Context */
    JSValueConst func;        /* 要执行的函数（只读）*/
    JSValueConst this_val;    /* this（只读）*/
    int argc;                 /* 参数数量 */
    JSValueConst *argv;       /* 参数（只读）*/
    JSValue result;           /* 返回值（可写）*/
    int yielded;              /* 是否 yield 了 */
    void *yield_data;         /* yield 的数据 */
} js_exec_context;

/* 在协程中执行 JS 函数 */
int stackful_call_js(stackful_schedule *S, js_exec_context *exec);

/* 启用 stackful 支持（注册 JS API） */
int stackful_enable_js_api(JSContext *ctx, stackful_schedule *S);

/* 获取全局调度器（用于在 C 函数中访问） */
stackful_schedule* stackful_get_global_schedule(void);

/* ========== JS 可调用的 API ========== */

/*
 * 在 JS 中调用：
 *
 * // 创建协程执行函数
 * const result = Stackful.call(function() {
 *     const r = remoteCall();  // 这里会在 C 层 yield
 *     return r * 2;
 * });
 *
 * // Yield（内部由 jtask.call 等调用）
 * Stackful.yield(data);
 */

#ifdef __cplusplus
}
#endif

#endif /* QUICKJS_STACKFUL_H */
