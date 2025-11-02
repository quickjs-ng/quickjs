/**
 * quickjs_stackful.c
 * QuickJS Stackful Coroutine Implementation
 *
 * 基于 ucontext 实现真正的 stackful 协程
 * 参考：云风的 coroutine.c
 */

#define _XOPEN_SOURCE 700  /* 启用 ucontext */

#include "quickjs_stackful.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

/* macOS 已弃用 ucontext，需要手动声明 */
#ifdef __APPLE__
extern int getcontext(ucontext_t *);
extern int setcontext(const ucontext_t *);
extern void makecontext(ucontext_t *, void (*)(), int, ...);
extern int swapcontext(ucontext_t *, const ucontext_t *);
#endif

#define DEFAULT_COROUTINE 16

/* ========== 协程结构定义 ========== */

struct stackful_coroutine {
    stackful_func func;      /* C 协程函数 */
    void *ud;                /* 用户数据 */
    ucontext_t ctx;          /* CPU 上下文 */
    stackful_schedule *sch;  /* 所属调度器 */
    ptrdiff_t cap;           /* 栈容量 */
    ptrdiff_t size;          /* 栈大小 */
    StackfulStatus status;   /* 状态 */
    char *stack;             /* 保存的栈 */

    /* QuickJS 专用 */
    JSContext *js_ctx;       /* 专属 Context */
};

/* ========== 内部辅助函数 ========== */

static struct stackful_coroutine* _co_new(
    stackful_schedule *S,
    stackful_func func,
    void *ud
) {
    struct stackful_coroutine *co = malloc(sizeof(*co));
    co->func = func;
    co->ud = ud;
    co->sch = S;
    co->cap = 0;
    co->size = 0;
    co->status = STACKFUL_READY;
    co->stack = NULL;
    co->js_ctx = NULL;
    return co;
}

static void _co_delete(struct stackful_coroutine *co) {
    if (co->stack) {
        free(co->stack);
    }
    if (co->js_ctx) {
        JS_FreeContext(co->js_ctx);
    }
    free(co);
}

/* 协程入口函数（包装） */
static void _mainfunc(uint32_t low32, uint32_t hi32) {
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
    stackful_schedule *S = (stackful_schedule *)ptr;
    int id = S->running;
    struct stackful_coroutine *C = S->co[id];

    /* 执行协程函数 */
    C->func(S, C->ud);

    /* 协程结束，清理 */
    _co_delete(C);
    S->co[id] = NULL;
    --S->nco;
    S->running = -1;
}

/* 保存栈 */
static void _save_stack(struct stackful_coroutine *C, char *top) {
    char dummy = 0;
    assert(top - &dummy <= STACKFUL_STACK_SIZE);

    if (C->cap < top - &dummy) {
        free(C->stack);
        C->cap = top - &dummy;
        C->stack = malloc(C->cap);
    }

    C->size = top - &dummy;
    memcpy(C->stack, &dummy, C->size);
}

/* ========== 调度器 API ========== */

stackful_schedule* stackful_open(JSRuntime *rt, JSContext *main_ctx) {
    stackful_schedule *S = malloc(sizeof(*S));
    S->nco = 0;
    S->cap = DEFAULT_COROUTINE;
    S->running = -1;
    S->co = malloc(sizeof(struct stackful_coroutine *) * S->cap);
    memset(S->co, 0, sizeof(struct stackful_coroutine *) * S->cap);

    S->rt = rt;
    S->main_ctx = main_ctx;

    return S;
}

void stackful_close(stackful_schedule *S) {
    for (int i = 0; i < S->cap; i++) {
        struct stackful_coroutine *co = S->co[i];
        if (co) {
            _co_delete(co);
        }
    }
    free(S->co);
    S->co = NULL;
    free(S);
}

int stackful_new(stackful_schedule *S, stackful_func func, void *ud) {
    struct stackful_coroutine *co = _co_new(S, func, ud);

    if (S->nco >= S->cap) {
        /* 扩容 */
        int id = S->cap;
        S->co = realloc(S->co, S->cap * 2 * sizeof(struct stackful_coroutine *));
        memset(S->co + S->cap, 0, sizeof(struct stackful_coroutine *) * S->cap);
        S->co[S->cap] = co;
        S->cap *= 2;
        ++S->nco;
        return id;
    } else {
        /* 找空位 */
        for (int i = 0; i < S->cap; i++) {
            int id = (i + S->nco) % S->cap;
            if (S->co[id] == NULL) {
                S->co[id] = co;
                ++S->nco;
                return id;
            }
        }
    }

    assert(0);
    return -1;
}

void stackful_resume(stackful_schedule *S, int id) {
    assert(S->running == -1);
    assert(id >= 0 && id < S->cap);

    struct stackful_coroutine *C = S->co[id];
    if (C == NULL) {
        printf("[DEBUG] resume: 协程 %d 不存在\n", id);
        return;
    }

    int status = C->status;
    printf("[DEBUG] resume: 协程 %d, 状态 %d\n", id, status);

    switch (status) {
    case STACKFUL_READY:
        /* 首次运行 */
        printf("[DEBUG] 首次运行，创建上下文\n");
        getcontext(&C->ctx);
        C->ctx.uc_stack.ss_sp = S->stack;
        C->ctx.uc_stack.ss_size = STACKFUL_STACK_SIZE;
        C->ctx.uc_link = &S->main;
        S->running = id;
        C->status = STACKFUL_RUNNING;

        /* 传递调度器指针 */
        uintptr_t ptr = (uintptr_t)S;
        makecontext(&C->ctx, (void (*)(void))_mainfunc, 2,
                   (uint32_t)ptr, (uint32_t)(ptr >> 32));

        printf("[DEBUG] swapcontext to coro\n");
        swapcontext(&S->main, &C->ctx);
        printf("[DEBUG] 返回主函数\n");
        break;

    case STACKFUL_SUSPENDED:
        /* 恢复运行 */
        printf("[DEBUG] 恢复挂起的协程，栈大小: %td\n", C->size);
        memcpy(S->stack + STACKFUL_STACK_SIZE - C->size, C->stack, C->size);
        S->running = id;
        C->status = STACKFUL_RUNNING;
        printf("[DEBUG] swapcontext to resume\n");
        swapcontext(&S->main, &C->ctx);
        printf("[DEBUG] 返回主函数（从 resume）\n");
        break;

    default:
        printf("[DEBUG] 未知状态: %d\n", status);
        assert(0);
    }
}

void stackful_yield(stackful_schedule *S) {
    int id = S->running;
    printf("[DEBUG] yield: 协程 %d\n", id);
    assert(id >= 0);

    struct stackful_coroutine *C = S->co[id];
    assert((char *)&C > S->stack);

    printf("[DEBUG] 保存栈...\n");
    _save_stack(C, S->stack + STACKFUL_STACK_SIZE);
    printf("[DEBUG] 栈已保存，大小: %td\n", C->size);

    C->status = STACKFUL_SUSPENDED;
    S->running = -1;

    printf("[DEBUG] swapcontext 回主函数\n");
    swapcontext(&C->ctx, &S->main);
    printf("[DEBUG] yield 返回了（被 resume 了）\n");
}

int stackful_status(stackful_schedule *S, int id) {
    assert(id >= 0 && id < S->cap);
    if (S->co[id] == NULL) {
        return STACKFUL_DEAD;
    }
    return S->co[id]->status;
}

int stackful_running(stackful_schedule *S) {
    return S->running;
}

/* ========== QuickJS 集成 ========== */

/* 协程中执行 JS 函数的包装 */
typedef struct {
    js_exec_context *exec;
} js_coro_wrapper;

static void _js_coro_func(stackful_schedule *S, void *ud) {
    js_coro_wrapper *wrapper = (js_coro_wrapper *)ud;
    js_exec_context *exec = wrapper->exec;

    /* 执行 JS 函数 */
    exec->result = JS_Call(
        exec->ctx,
        exec->func,
        exec->this_val,
        exec->argc,
        exec->argv
    );

    /* 检查是否异常 */
    if (JS_IsException(exec->result)) {
        /* 异常处理 */
    }

    exec->yielded = 0;
    free(wrapper);
}

int stackful_call_js(stackful_schedule *S, js_exec_context *exec) {
    js_coro_wrapper *wrapper = malloc(sizeof(js_coro_wrapper));
    wrapper->exec = exec;

    /* 创建协程 */
    int coro_id = stackful_new(S, _js_coro_func, wrapper);

    /* 为协程创建专属 Context */
    struct stackful_coroutine *co = S->co[coro_id];
    co->js_ctx = JS_NewContext(S->rt);
    exec->ctx = co->js_ctx;

    /* 在 Context 的 opaque 中存储 coro_id */
    JS_SetContextOpaque(co->js_ctx, (void *)(intptr_t)coro_id);

    /* 运行协程 */
    stackful_resume(S, coro_id);

    return coro_id;
}

/* ========== JS API 实现 ========== */

/* 全局调度器（简化，实际应该和 Runtime 绑定） */
static stackful_schedule *g_stackful_schedule = NULL;

/* JS: Stackful.yield(data) */
static JSValue js_stackful_yield(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    if (!g_stackful_schedule) {
        return JS_ThrowInternalError(ctx, "Stackful not initialized");
    }

    int coro_id = stackful_running(g_stackful_schedule);
    if (coro_id < 0) {
        return JS_ThrowInternalError(ctx, "Not in a coroutine");
    }

    /* TODO: 保存 yield 的数据 */

    /* Yield */
    stackful_yield(g_stackful_schedule);

    /* Resume 后返回 */
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

/* JS: Stackful.status(coro_id) */
static JSValue js_stackful_status(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    if (!g_stackful_schedule) {
        return JS_UNDEFINED;
    }

    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "需要协程 ID");
    }

    int coro_id;
    if (JS_ToInt32(ctx, &coro_id, argv[0]) < 0) {
        return JS_EXCEPTION;
    }

    int status = stackful_status(g_stackful_schedule, coro_id);
    return JS_NewInt32(ctx, status);
}

int stackful_enable_js_api(JSContext *ctx, stackful_schedule *S) {
    g_stackful_schedule = S;

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue stackful_obj = JS_NewObject(ctx);

    /* 注册方法 */
    JS_SetPropertyStr(ctx, stackful_obj, "yield",
                     JS_NewCFunction(ctx, js_stackful_yield, "yield", 1));

    JS_SetPropertyStr(ctx, stackful_obj, "running",
                     JS_NewCFunction(ctx, js_stackful_running, "running", 0));

    JS_SetPropertyStr(ctx, stackful_obj, "status",
                     JS_NewCFunction(ctx, js_stackful_status, "status", 1));

    /* 常量 */
    JS_SetPropertyStr(ctx, stackful_obj, "DEAD",
                     JS_NewInt32(ctx, STACKFUL_DEAD));
    JS_SetPropertyStr(ctx, stackful_obj, "READY",
                     JS_NewInt32(ctx, STACKFUL_READY));
    JS_SetPropertyStr(ctx, stackful_obj, "RUNNING",
                     JS_NewInt32(ctx, STACKFUL_RUNNING));
    JS_SetPropertyStr(ctx, stackful_obj, "SUSPENDED",
                     JS_NewInt32(ctx, STACKFUL_SUSPENDED));

    JS_SetPropertyStr(ctx, global, "Stackful", stackful_obj);
    JS_FreeValue(ctx, global);

    return 0;
}

stackful_schedule* stackful_get_global_schedule(void) {
    return g_stackful_schedule;
}
