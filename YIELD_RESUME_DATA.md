# Yield/Resume 数据传递机制

## Lua 的方式（ltask）

```lua
function ltask.call(address, ...)
    -- 发送消息
    post_message(address, session, type, msg, sz)

    -- Yield 等待
    local type, session, msg, sz = coroutine.yield()
    -- ↑ resume 时传入的参数直接返回

    if type == MESSAGE_RESPONSE then
        return unpack(msg, sz)
    end
end

-- 消息到达时
function on_message(from, session, type, msg, sz)
    local co = session_map[session]
    coroutine.resume(co, type, session, msg, sz)  -- 传参
    --                   ^^^^^^^^^^^^^^^^^^^^^ 这些参数会被 yield 接收
end
```

## minicoro 的方式（jtask）

```c
// jtask.call 的 C 实现
static JSValue js_jtask_call(JSContext *ctx, ...) {
    // 1. 发送消息
    send_message(address, session, type, msg, sz);

    // 2. Yield
    stackful_schedule *S = stackful_get_global_schedule();
    stackful_yield(S);

    // 3. Resume 后，pop 数据
    char buffer[1024];
    size_t bytes = mco_get_bytes_stored(co);
    mco_pop(co, buffer, bytes);

    // 4. 解析消息
    int type, session;
    void *msg;
    size_t sz;
    parse_message(buffer, &type, &session, &msg, &sz);

    if (type == MESSAGE_RESPONSE) {
        return unpack_js_value(ctx, msg, sz);
    }
}

// 消息到达时
void on_message_received(int session, int type, void *msg, size_t sz) {
    int coro_id = session_map[session];

    // 1. Pack 消息
    char buffer[1024];
    size_t len = pack_message(buffer, type, session, msg, sz);

    // 2. Push 数据
    mco_coro *co = S->coroutines[coro_id];
    mco_push(co, buffer, len);

    // 3. Resume
    stackful_resume(S, coro_id);
}
```

## 包装成更友好的 API

可以封装一下：

```c
// 包装的 yield（传出数据）
void stackful_yield_with_data(stackful_schedule *S, void *data, size_t len) {
    int id = S->running;
    mco_coro *co = S->coroutines[id];

    if (data && len > 0) {
        mco_push(co, data, len);
    }

    mco_yield(co);
}

// 包装的 resume（传入数据）
int stackful_resume_with_data(stackful_schedule *S, int id, void *data, size_t len) {
    mco_coro *co = S->coroutines[id];

    if (data && len > 0) {
        mco_push(co, data, len);
    }

    S->running = id;
    mco_result res = mco_resume(co);

    // ... 处理状态

    return res == MCO_SUCCESS ? 0 : -1;
}

// Pop 数据的辅助函数
void* stackful_pop_data(stackful_schedule *S, size_t *out_len) {
    int id = S->running;
    mco_coro *co = S->coroutines[id];

    size_t bytes = mco_get_bytes_stored(co);
    if (bytes == 0) {
        *out_len = 0;
        return NULL;
    }

    void *data = malloc(bytes);
    mco_pop(co, data, bytes);
    *out_len = bytes;
    return data;
}
```

## 使用示例

```c
// jtask.call 简化版
static JSValue js_jtask_call(JSContext *ctx, ...) {
    // 发送消息
    send_message(...);

    // Yield（不传数据）
    stackful_yield_with_data(S, NULL, 0);

    // Resume 后 pop 数据
    size_t len;
    void *data = stackful_pop_data(S, &len);

    // 解析为 JS 值
    JSValue result = parse_message_to_js(ctx, data, len);
    free(data);

    return result;
}

// 消息处理
void on_message(int session, void *msg, size_t sz) {
    int coro_id = session_map[session];

    // Resume 并传入数据
    stackful_resume_with_data(S, coro_id, msg, sz);
}
```

## 测试代码

见 `test_yield_data.c`：

```bash
gcc -o test_yield_data test_yield_data.c -I../minicoro
./test_yield_data
```

输出：
```
>>> Resume 2 (传数据)
[Coro] 存储有 15 字节
[Coro] 收到数据: 'data from main'
[Coro] 发送数据并 yield
主函数：存储有 19 字节
主函数收到: 'response from coro'
```

## 总结

| 机制 | Lua | minicoro |
|------|-----|----------|
| 传数据给 yield | `resume(co, data)` | `push + resume` |
| Yield 返回数据 | `return yield(data)` | `push + yield` |
| 接收数据 | `local x = yield()` | `pop` after resume/yield |
| 优点 | 简洁 | 灵活（任意大小） |
| 缺点 | 固定类型 | 需要手动管理 |

**结论**：minicoro 的 push/pop 机制**完全可以实现 ltask 的消息传递**！
