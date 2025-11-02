# QuickJS Generator/Coroutine Support

这是 QuickJS 的协程扩展版本，专门为 JTask 项目提供 Generator 自动恢复支持。

## 特性

- ✅ **Generator 自动恢复**：类似 Lua 协程的 yield/resume
- ✅ **Session 管理**：自动跟踪等待的 Generator
- ✅ **线程安全**：支持多线程环境
- ✅ **零侵入**：不修改 QuickJS 原有代码

## 文件说明

```
quickjs_generator/
├── quickjs_coroutine.h    # 协程扩展 API
├── quickjs_coroutine.c    # 协程实现
├── quickjs.*              # 原版 QuickJS 文件（未修改）
└── Makefile.coroutine     # 带协程支持的编译配置
```

## 编译方法

### 方法1：使用 Makefile

```bash
make -f Makefile.coroutine
```

### 方法2：手动编译

```bash
# 编译所有对象文件
clang -c -O2 -Wall quickjs.c -o quickjs.o
clang -c -O2 -Wall quickjs_coroutine.c -o quickjs_coroutine.o
clang -c -O2 -Wall cutils.c -o cutils.o
clang -c -O2 -Wall libunicode.c -o libunicode.o
clang -c -O2 -Wall libregexp.c -o libregexp.o

# 创建静态库
ar rcs libquickjs_generator.a \
    quickjs.o \
    quickjs_coroutine.o \
    cutils.o \
    libunicode.o \
    libregexp.o
```

## 在 JTask 中使用

### 1. 修改 jtask 的 Makefile

```makefile
# 使用 generator 版本的 QuickJS
QUICKJS_DIR = ../3rd/quickjs_generator
QUICKJS_LIB = $(QUICKJS_DIR)/libquickjs_generator.a

# 包含路径
CFLAGS += -I$(QUICKJS_DIR)
```

### 2. 在 service.c 中启用

```c
#include "quickjs.h"
#include "quickjs_coroutine.h"

static JSCoroutineManager *g_coro_mgr = NULL;

void service_init() {
    // 初始化协程管理器
    g_coro_mgr = JS_NewCoroutineManager(runtime);
}

void service_create(service_t *svc) {
    // 为服务启用协程
    JS_EnableCoroutines(svc->ctx, g_coro_mgr);
}

// 处理 Generator yield
void service_handle_yield(service_t *svc, JSValue yielded) {
    // 检查是否是 jtask.call
    JSValue is_call = JS_GetPropertyStr(svc->ctx, yielded, "__jtask_call__");

    if (JS_ToBool(svc->ctx, is_call)) {
        // 生成 session
        int session = JS_CoroutineGenerateSession(g_coro_mgr);

        // 保存 generator
        JS_CoroutineWait(g_coro_mgr, svc->ctx, svc->current_gen, session, svc->id);

        // 发送消息...
    }

    JS_FreeValue(svc->ctx, is_call);
}

// 处理响应
void service_handle_response(int session, JSValue data) {
    // 自动恢复 generator
    JS_CoroutineResume(g_coro_mgr, session, data);
}
```

### 3. JS 代码示例

```javascript
// 现在可以用 Generator 了！
function* serviceA() {
    console.log("Service A: starting");

    // yield 会自动保存状态
    const result = yield {
        __jtask_call__: true,
        target: 'serviceB',
        cmd: 'getData'
    };

    // 响应到达时自动恢复
    console.log("Service A: got result", result);

    return result;
}
```

## API 参考

### C API

```c
// 创建协程管理器
JSCoroutineManager* JS_NewCoroutineManager(JSRuntime *rt);

// 启用协程支持
int JS_EnableCoroutines(JSContext *ctx, JSCoroutineManager *mgr);

// 保存等待的 Generator
int JS_CoroutineWait(mgr, ctx, generator, session_id, service_id);

// 恢复 Generator
int JS_CoroutineResume(mgr, session_id, data);

// 生成 Session ID
int JS_CoroutineGenerateSession(mgr);

// 检查是否是 Generator
int JS_IsGenerator(ctx, value);
```

### JS API（自动注册）

```javascript
__coroutine_wait(generator, session, service_id)  // 保存 generator
__coroutine_resume(session, data)                  // 恢复 generator
__coroutine_session()                              // 生成 session ID
__is_generator(obj)                                // 检查是否 generator
```

## 测试

```bash
# 编译测试程序
make -f Makefile.coroutine test

# 运行测试
./test_coroutine
```

## 开发计划

- [x] Generator 自动恢复
- [x] Session 管理
- [x] 线程安全支持
- [ ] 错误处理优化
- [ ] 性能优化
- [ ] 调试工具

## 许可

与 QuickJS 相同的 MIT 许可证