/**
 * quickjs_stackful_mini.c
 * QuickJS Stackful Coroutine Implementation (based on Tina)
 * 
 * Migration: Minicoro → Tina (2025-11-01)
 */

#define TINA_IMPLEMENTATION
#include "quickjs_stackful_mini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_COROUTINE 16
#define TINA_DEFAULT_STACK_SIZE (1024*1024)  /* 1MB, safe for modern systems */

/* Debug flag - set to 0 for production */
#define STACKFUL_DEBUG 0

#if STACKFUL_DEBUG
#define DEBUG_LOG(...) DEBUG_LOG( __VA_ARGS__)
#else
#define DEBUG_LOG(...) ((void)0)
#endif

/* ========== Data storage functions ========== */

static int tina_storage_push(tina_storage *s, const void *data, size_t len) {
    if (s->size + len > sizeof(s->buffer)) {
        DEBUG_LOG("[tina_storage_push] ERROR: Buffer full (size=%zu, len=%zu, max=%zu)\n",
                s->size, len, sizeof(s->buffer));
        return -1;
    }
    memcpy(s->buffer + s->size, data, len);
    s->size += len;
    return 0;
}

static int tina_storage_pop(tina_storage *s, void *data, size_t len) {
    if (s->size < len) {
        DEBUG_LOG("[tina_storage_pop] ERROR: Not enough data (size=%zu, len=%zu)\n",
                s->size, len);
        return -1;
    }
    s->size -= len;
    memcpy(data, s->buffer + s->size, len);
    return 0;
}

static size_t tina_storage_bytes(const tina_storage *s) {
    return s->size;
}

static void tina_storage_clear(tina_storage *s) {
    s->size = 0;
}

/* ========== Tina entry wrapper ========== */

static void* tina_entry_wrapper(tina *coro, void *value) {
    tina_wrapper *wrapper = (tina_wrapper*)coro->user_data;
    
    DEBUG_LOG( "[tina_entry_wrapper] Coroutine starting\n");
    
    /* Call the original function with the original user_data */
    wrapper->user_func(wrapper->user_data);
    
    DEBUG_LOG( "[tina_entry_wrapper] Coroutine finished\n");
    
    return NULL;
}

/* ========== Scheduler implementation ========== */

stackful_schedule* stackful_open(JSRuntime *rt, JSContext *main_ctx) {
    stackful_schedule *S = malloc(sizeof(*S));
    if (!S) {
        DEBUG_LOG( "[stackful_open] ERROR: Failed to allocate scheduler\n");
        return NULL;
    }
    
    S->rt = rt;
    S->main_ctx = main_ctx;
    S->cap = DEFAULT_COROUTINE;
    S->count = 0;
    S->running = -1;
    
    S->coroutines = calloc(S->cap, sizeof(tina_wrapper*));
    S->storages = calloc(S->cap, sizeof(tina_storage));
    
    if (!S->coroutines || !S->storages) {
        DEBUG_LOG( "[stackful_open] ERROR: Failed to allocate arrays\n");
        free(S->coroutines);
        free(S->storages);
        free(S);
        return NULL;
    }
    
    // DEBUG_LOG( "[stackful_open] Scheduler created (cap=%d)\n", S->cap);
    return S;
}

void stackful_close(stackful_schedule *S) {
    if (!S) return;
    
    DEBUG_LOG( "[stackful_close] Destroying scheduler\n");
    
    for (int i = 0; i < S->cap; i++) {
        if (S->coroutines[i]) {
            tina_wrapper *wrapper = S->coroutines[i];
            if (wrapper->coro) {
                /* Tina's coro pointer points INTO buffer, so only free buffer */
                if (wrapper->coro->buffer) {
                    free(wrapper->coro->buffer);
                }
                /* Don't free wrapper->coro itself - it's inside the buffer! */
            }
            free(wrapper);
        }
    }
    
    free(S->coroutines);
    free(S->storages);
    free(S);
    
    DEBUG_LOG( "[stackful_close] Scheduler destroyed\n");
}

int stackful_new(stackful_schedule *S, stackful_func func, void *ud) {
    if (!S || !func) {
        DEBUG_LOG( "[stackful_new] ERROR: Invalid arguments\n");
        return -1;
    }
    
    /* Find empty slot */
    int id = -1;
    for (int i = 0; i < S->cap; i++) {
        if (S->coroutines[i] == NULL) {
            id = i;
            break;
        }
    }
    
    /* Expand if needed */
    if (id == -1) {
        id = S->cap;
        int new_cap = S->cap * 2;
        
        tina_wrapper **new_coros = realloc(S->coroutines, new_cap * sizeof(tina_wrapper*));
        tina_storage *new_storages = realloc(S->storages, new_cap * sizeof(tina_storage));
        
        if (!new_coros || !new_storages) {
            DEBUG_LOG( "[stackful_new] ERROR: Failed to expand capacity\n");
            return -1;
        }
        
        S->coroutines = new_coros;
        S->storages = new_storages;
        
        /* Zero new entries */
        memset(S->coroutines + S->cap, 0, (new_cap - S->cap) * sizeof(tina_wrapper*));
        memset(S->storages + S->cap, 0, (new_cap - S->cap) * sizeof(tina_storage));
        
        S->cap = new_cap;
        
        DEBUG_LOG( "[stackful_new] Expanded capacity to %d\n", new_cap);
    }
    
    /* Create wrapper */
    tina_wrapper *wrapper = malloc(sizeof(tina_wrapper));
    if (!wrapper) {
        DEBUG_LOG( "[stackful_new] ERROR: Failed to allocate wrapper\n");
        return -1;
    }
    
    wrapper->user_data = ud;
    wrapper->user_func = func;
    wrapper->status = STACKFUL_STATUS_SUSPENDED;
    wrapper->yield_count = 0;
    
    /* Create Tina coroutine */
    wrapper->coro = tina_init(NULL, TINA_DEFAULT_STACK_SIZE, tina_entry_wrapper, wrapper);
    
    if (!wrapper->coro) {
        DEBUG_LOG( "[stackful_new] ERROR: tina_init failed\n");
        free(wrapper);
        return -1;
    }
    
    S->coroutines[id] = wrapper;
    S->count++;
    
    DEBUG_LOG( "[stackful_new] Created coroutine id=%d (count=%d)\n", id, S->count);
    
    return id;
}

int stackful_resume(stackful_schedule *S, int id) {
    if (!S || id < 0 || id >= S->cap || !S->coroutines[id]) {
        DEBUG_LOG( "[stackful_resume] ERROR: Invalid coroutine id=%d\n", id);
        return -1;
    }
    
    tina_wrapper *wrapper = S->coroutines[id];
    
    if (wrapper->status == STACKFUL_STATUS_DEAD) {
        DEBUG_LOG( "[stackful_resume] ERROR: Coroutine id=%d already dead\n", id);
        return -1;
    }
    
    /* Save caller context */
    int caller_id = S->running;
    
    DEBUG_LOG( "[stackful_resume] caller_id=%d, resuming coro_id=%d, status=%d\n",
            caller_id, id, wrapper->status);
    
    /* Update status */
    S->running = id;
    wrapper->status = STACKFUL_STATUS_RUNNING;
    
    /* Resume Tina coroutine */
    void *result = tina_resume(wrapper->coro, NULL);
    
    DEBUG_LOG( "[stackful_resume] tina_resume(%d) returned\n", id);
    
    /* Check completion */
    if (wrapper->coro->completed) {
        DEBUG_LOG( "[stackful_resume] Coroutine %d completed, cleaning up\n", id);
        
        wrapper->status = STACKFUL_STATUS_DEAD;
        
        /* Cleanup - Tina's coro is inside buffer, only free buffer */
        if (wrapper->coro->buffer) {
            free(wrapper->coro->buffer);
        }
        /* Don't free wrapper->coro - it points inside the buffer! */
        free(wrapper);
        
        S->coroutines[id] = NULL;
        S->count--;
        
        DEBUG_LOG( "[stackful_resume] Coroutine %d destroyed (count=%d)\n", id, S->count);
    } else {
        /* Yielded */
        wrapper->status = STACKFUL_STATUS_SUSPENDED;
        wrapper->yield_count++;
        
        DEBUG_LOG( "[stackful_resume] Coroutine %d suspended (yield_count=%d)\n",
                id, wrapper->yield_count);
    }
    
    /* Restore caller context */
    S->running = caller_id;
    
    DEBUG_LOG( "[stackful_resume] restored caller_id=%d\n", caller_id);
    
    return 0;
}

void stackful_yield(stackful_schedule *S) {
    if (!S) {
        DEBUG_LOG( "[stackful_yield] ERROR: NULL scheduler\n");
        return;
    }
    
    int id = S->running;
    if (id < 0) {
        DEBUG_LOG( "[stackful_yield] ERROR: Not running in coroutine (running=%d)\n", id);
        return;
    }
    
    if (id >= S->cap || !S->coroutines[id]) {
        DEBUG_LOG( "[stackful_yield] ERROR: Invalid coroutine id=%d\n", id);
        return;
    }
    
    tina_wrapper *wrapper = S->coroutines[id];
    
    DEBUG_LOG( "[stackful_yield] coro_id=%d about to yield\n", id);
    
    /* Tina yield */
    tina_yield(wrapper->coro, NULL);
    
    DEBUG_LOG( "[stackful_yield] coro_id=%d resumed after yield\n", id);
}

void stackful_yield_with_flag(stackful_schedule *S, int flag) {
    if (!S) {
        DEBUG_LOG( "[stackful_yield_with_flag] ERROR: NULL scheduler\n");
        return;
    }
    
    int id = S->running;
    if (id < 0 || id >= S->cap || !S->coroutines[id]) {
        DEBUG_LOG( "[stackful_yield_with_flag] ERROR: Invalid running id=%d\n", id);
        return;
    }
    
    tina_wrapper *wrapper = S->coroutines[id];
    tina_storage *storage = &S->storages[id];
    
    DEBUG_LOG( "[stackful_yield_with_flag] coro_id=%d pushing flag=%d before yield\n",
            id, flag);
    
    /* Push flag to storage */
    if (tina_storage_push(storage, &flag, sizeof(int)) < 0) {
        DEBUG_LOG( "[stackful_yield_with_flag] ERROR: Failed to push flag\n");
        return;
    }
    
    /* Yield */
    tina_yield(wrapper->coro, NULL);
    
    DEBUG_LOG( "[stackful_yield_with_flag] coro_id=%d resumed after yield\n", id);
}

int stackful_pop_continue_flag(stackful_schedule *S, int id) {
    if (!S || id < 0 || id >= S->cap || !S->coroutines[id]) {
        return 0;  /* No flag */
    }
    
    tina_storage *storage = &S->storages[id];
    size_t bytes = tina_storage_bytes(storage);
    
    DEBUG_LOG( "[stackful_pop_continue_flag] coro_id=%d has %zu bytes stored\n",
            id, bytes);
    
    if (bytes != sizeof(int)) {
        return 0;  /* No flag or wrong size */
    }
    
    int flag = 0;
    if (tina_storage_pop(storage, &flag, sizeof(int)) < 0) {
        DEBUG_LOG( "[stackful_pop_continue_flag] ERROR: Failed to pop flag\n");
        return 0;
    }
    
    DEBUG_LOG( "[stackful_pop_continue_flag] coro_id=%d popped flag=%d\n", id, flag);
    return flag;
}

stackful_status_t stackful_status(stackful_schedule *S, int id) {
    if (!S || id < 0 || id >= S->cap || !S->coroutines[id]) {
        return STACKFUL_STATUS_DEAD;
    }
    
    return S->coroutines[id]->status;
}

int stackful_running(stackful_schedule *S) {
    if (!S) return -1;
    return S->running;
}

/* ========== QuickJS Integration ========== */

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

    stackful_status_t status = stackful_status(g_stackful_schedule, id);
    return JS_NewInt32(ctx, (int)status);
}

int stackful_enable_js_api(JSContext *ctx, stackful_schedule *S) {
    if (!ctx || !S) {
        DEBUG_LOG( "[stackful_enable_js_api] ERROR: Invalid arguments\n");
        return -1;
    }
    
    g_stackful_schedule = S;

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue stackful_obj = JS_NewObject(ctx);

    /* Register methods */
    JS_SetPropertyStr(ctx, stackful_obj, "yield",
                     JS_NewCFunction(ctx, js_stackful_yield, "yield", 0));

    JS_SetPropertyStr(ctx, stackful_obj, "running",
                     JS_NewCFunction(ctx, js_stackful_running, "running", 0));

    JS_SetPropertyStr(ctx, stackful_obj, "status",
                     JS_NewCFunction(ctx, js_stackful_status, "status", 1));

    /* Status constants */
    JS_SetPropertyStr(ctx, stackful_obj, "DEAD",
                     JS_NewInt32(ctx, STACKFUL_STATUS_DEAD));
    JS_SetPropertyStr(ctx, stackful_obj, "NORMAL",
                     JS_NewInt32(ctx, STACKFUL_STATUS_NORMAL));
    JS_SetPropertyStr(ctx, stackful_obj, "RUNNING",
                     JS_NewInt32(ctx, STACKFUL_STATUS_RUNNING));
    JS_SetPropertyStr(ctx, stackful_obj, "SUSPENDED",
                     JS_NewInt32(ctx, STACKFUL_STATUS_SUSPENDED));

    JS_SetPropertyStr(ctx, global, "Stackful", stackful_obj);
    JS_FreeValue(ctx, global);
    
    DEBUG_LOG( "[stackful_enable_js_api] JS API registered\n");

    return 0;
}

stackful_schedule* stackful_get_global_schedule(void) {
    return g_stackful_schedule;
}
