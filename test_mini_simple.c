/**
 * test_mini_simple.c
 * 测试 minicoro 版本的 stackful 协程（纯 C，不涉及 QuickJS）
 */

#define MINICORO_IMPL
#include "../minicoro/minicoro.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int cap;
    int count;
    int running;
    mco_coro **coroutines;
} simple_schedule;

simple_schedule* simple_open() {
    simple_schedule *S = malloc(sizeof(*S));
    S->cap = 16;
    S->count = 0;
    S->running = -1;
    S->coroutines = calloc(S->cap, sizeof(mco_coro*));
    return S;
}

void simple_close(simple_schedule *S) {
    for (int i = 0; i < S->cap; i++) {
        if (S->coroutines[i]) {
            mco_destroy(S->coroutines[i]);
        }
    }
    free(S->coroutines);
    free(S);
}

int simple_new(simple_schedule *S, void (*func)(mco_coro*), void *ud) {
    for (int i = 0; i < S->cap; i++) {
        if (S->coroutines[i] == NULL) {
            mco_desc desc = mco_desc_init(func, 0);
            desc.user_data = ud;
            if (mco_create(&S->coroutines[i], &desc) == MCO_SUCCESS) {
                S->count++;
                return i;
            }
            return -1;
        }
    }
    return -1;
}

int simple_resume(simple_schedule *S, int id) {
    if (id < 0 || id >= S->cap || !S->coroutines[id]) return -1;
    S->running = id;
    mco_result res = mco_resume(S->coroutines[id]);
    if (mco_status(S->coroutines[id]) == MCO_DEAD) {
        mco_destroy(S->coroutines[id]);
        S->coroutines[id] = NULL;
        S->count--;
    }
    S->running = -1;
    return res == MCO_SUCCESS ? 0 : -1;
}

mco_state simple_status(simple_schedule *S, int id) {
    if (id < 0 || id >= S->cap || !S->coroutines[id]) return MCO_DEAD;
    return mco_status(S->coroutines[id]);
}

int step = 0;

/* 简单的协程函数 */
void simple_coro(mco_coro* co) {
    printf("[Coro] Step 1\n");
    step++;

    mco_yield(co);

    printf("[Coro] Step 2 (after first resume)\n");
    step++;

    mco_yield(co);

    printf("[Coro] Step 3 (final)\n");
    step++;
}

int main() {
    printf("=== Minicoro Stackful Test ===\n\n");

    simple_schedule *S = simple_open();

    int coro_id = simple_new(S, simple_coro, NULL);
    printf("Created coroutine ID: %d\n\n", coro_id);

    printf(">>> Resume 1\n");
    simple_resume(S, coro_id);
    printf("Status: %d, Step: %d\n\n", simple_status(S, coro_id), step);

    printf(">>> Resume 2\n");
    simple_resume(S, coro_id);
    printf("Status: %d, Step: %d\n\n", simple_status(S, coro_id), step);

    printf(">>> Resume 3\n");
    simple_resume(S, coro_id);
    printf("Status: %d, Step: %d\n\n", simple_status(S, coro_id), step);

    simple_close(S);

    printf("Expected: step=3, Got: step=%d\n", step);
    printf(step == 3 ? "✅ PASS\n" : "❌ FAIL\n");

    return step == 3 ? 0 : 1;
}
