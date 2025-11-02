/**
 * test_tina_basic.c
 * Basic test for Tina-based stackful coroutines
 */

#include "quickjs_stackful_mini.h"
#include <stdio.h>
#include <assert.h>

/* Global scheduler for testing */
static stackful_schedule *S = NULL;

/* Test coroutine 1: Simple yield */
void test_coro_simple(void *arg) {
    printf("[test_coro_simple] Starting\n");
    
    printf("[test_coro_simple] First yield\n");
    stackful_yield(S);
    
    printf("[test_coro_simple] Second yield\n");
    stackful_yield(S);
    
    printf("[test_coro_simple] Exiting\n");
}

/* Test coroutine 2: Multiple yields in loop */
void test_coro_loop(void *arg) {
    int *count_ptr = (int*)arg;
    
    printf("[test_coro_loop] Starting, count=%d\n", *count_ptr);
    
    for (int i = 0; i < *count_ptr; i++) {
        printf("[test_coro_loop] Iteration %d, yielding\n", i);
        stackful_yield(S);
    }
    
    printf("[test_coro_loop] Finished %d iterations\n", *count_ptr);
}

/* Test coroutine 3: Yield with flag */
void test_coro_flag(void *arg) {
    printf("[test_coro_flag] Starting\n");
    
    printf("[test_coro_flag] Yielding with flag=1\n");
    stackful_yield_with_flag(S, 1);
    
    printf("[test_coro_flag] Yielding with flag=0\n");
    stackful_yield_with_flag(S, 0);
    
    printf("[test_coro_flag] Exiting\n");
}

int main() {
    printf("=== Tina Stackful Coroutine Basic Test ===\n\n");
    
    /* Test 1: Scheduler creation */
    printf("Test 1: Creating scheduler...\n");
    S = stackful_open(NULL, NULL);
    assert(S != NULL);
    printf("✓ Scheduler created\n\n");
    
    /* Test 2: Simple coroutine */
    printf("Test 2: Simple coroutine with 2 yields...\n");
    int id1 = stackful_new(S, test_coro_simple, NULL);
    assert(id1 >= 0);
    printf("✓ Coroutine created, id=%d\n", id1);
    
    stackful_status_t status = stackful_status(S, id1);
    printf("  Status before resume: %d (expected 3=SUSPENDED)\n", status);
    assert(status == STACKFUL_STATUS_SUSPENDED);
    
    printf("  Resuming (1st time)...\n");
    int ret = stackful_resume(S, id1);
    assert(ret == 0);
    status = stackful_status(S, id1);
    printf("  Status after 1st resume: %d (expected 3=SUSPENDED)\n", status);
    assert(status == STACKFUL_STATUS_SUSPENDED);
    
    printf("  Resuming (2nd time)...\n");
    ret = stackful_resume(S, id1);
    assert(ret == 0);
    status = stackful_status(S, id1);
    printf("  Status after 2nd resume: %d (expected 3=SUSPENDED)\n", status);
    assert(status == STACKFUL_STATUS_SUSPENDED);
    
    printf("  Resuming (3rd time, should complete)...\n");
    ret = stackful_resume(S, id1);
    assert(ret == 0);
    status = stackful_status(S, id1);
    printf("  Status after 3rd resume: %d (expected 0=DEAD)\n", status);
    assert(status == STACKFUL_STATUS_DEAD);
    
    printf("✓ Simple coroutine completed\n\n");
    
    /* Test 3: Loop coroutine */
    printf("Test 3: Loop coroutine with 5 yields...\n");
    int count = 5;
    int id2 = stackful_new(S, test_coro_loop, &count);
    assert(id2 >= 0);
    printf("✓ Coroutine created, id=%d\n", id2);
    
    for (int i = 0; i <= count; i++) {
        printf("  Resume iteration %d...\n", i);
        ret = stackful_resume(S, id2);
        assert(ret == 0);
        
        status = stackful_status(S, id2);
        if (i < count) {
            assert(status == STACKFUL_STATUS_SUSPENDED);
        } else {
            assert(status == STACKFUL_STATUS_DEAD);
        }
    }
    
    printf("✓ Loop coroutine completed\n\n");
    
    /* Test 4: Yield with flag */
    printf("Test 4: Coroutine with flag storage...\n");
    int id3 = stackful_new(S, test_coro_flag, NULL);
    assert(id3 >= 0);
    printf("✓ Coroutine created, id=%d\n", id3);
    
    printf("  Resuming (1st time)...\n");
    ret = stackful_resume(S, id3);
    assert(ret == 0);
    
    int flag = stackful_pop_continue_flag(S, id3);
    printf("  Popped flag=%d (expected 1)\n", flag);
    assert(flag == 1);
    
    printf("  Resuming (2nd time)...\n");
    ret = stackful_resume(S, id3);
    assert(ret == 0);
    
    flag = stackful_pop_continue_flag(S, id3);
    printf("  Popped flag=%d (expected 0)\n", flag);
    assert(flag == 0);
    
    printf("  Resuming (3rd time, should complete)...\n");
    ret = stackful_resume(S, id3);
    assert(ret == 0);
    
    status = stackful_status(S, id3);
    assert(status == STACKFUL_STATUS_DEAD);
    
    printf("✓ Flag storage test completed\n\n");
    
    /* Cleanup */
    printf("Test 5: Scheduler cleanup...\n");
    stackful_close(S);
    printf("✓ Scheduler destroyed\n\n");
    
    printf("=== All tests passed! ===\n");
    return 0;
}
