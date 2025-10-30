/**
 * test_yield_data.c
 * 测试 yield/resume 传递数据（类似 Lua）
 */

#define MINICORO_IMPL
#include "../minicoro/minicoro.h"
#include <stdio.h>
#include <string.h>

void coro_func(mco_coro* co) {
    printf("[Coro] 启动\n");

    // 第一次 yield
    mco_yield(co);

    // Resume 后立即 pop（主函数在 resume 前 push 了）
    char buffer[100];
    size_t bytes = mco_get_bytes_stored(co);
    printf("[Coro] 存储有 %zu 字节\n", bytes);

    if (bytes > 0) {
        mco_pop(co, buffer, bytes);
        printf("[Coro] 收到数据: '%s'\n", buffer);
    }

    // Push 数据，然后 yield
    const char *response = "response from coro";
    mco_push(co, response, strlen(response) + 1);
    printf("[Coro] 发送数据并 yield\n");
    mco_yield(co);

    printf("[Coro] 结束\n");
}

int main() {
    printf("=== Yield/Resume 传数据测试 ===\n\n");

    mco_desc desc = mco_desc_init(coro_func, 0);
    desc.storage_size = 1024;  // 分配存储空间

    mco_coro* co;
    mco_create(&co, &desc);

    // 第一次 resume - 启动协程
    printf(">>> Resume 1 (启动)\n");
    mco_resume(co);
    printf("协程状态: %d\n\n", mco_status(co));

    // 第二次 resume - 传数据给协程
    printf(">>> Resume 2 (传数据)\n");
    const char *data = "data from main";
    mco_push(co, data, strlen(data) + 1);  // Push 数据
    mco_resume(co);                         // Resume

    // 协程 yield 了，检查并 pop 数据
    size_t bytes = mco_get_bytes_stored(co);
    printf("主函数：存储有 %zu 字节\n", bytes);

    if (bytes > 0) {
        char response[100];
        mco_pop(co, response, bytes);
        printf("主函数收到: '%s'\n", response);
    }
    printf("协程状态: %d\n\n", mco_status(co));

    // 第三次 resume - 结束
    printf(">>> Resume 3 (结束)\n");
    mco_resume(co);
    printf("协程状态: %d\n\n", mco_status(co));

    mco_destroy(co);

    printf("=== 测试完成 ===\n");
    return 0;
}
