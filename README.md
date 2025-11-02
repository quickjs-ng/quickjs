# QuickJS 协程扩展使用说明书

## 一、核心概念

### 像 ltask 一样工作

ltask 的核心能力是让异步调用看起来像同步：

```javascript
// ltask 风格（我们现在可以实现的）
function* service_a() {
    // 看起来像同步，实际是异步
    const result = yield jtask.call("service_b", { data: 123 });
    console.log("得到结果：", result);
    return result * 2;
}
```

**无需回调地狱！无需 Promise 链！**

## 二、当前能力状态

### ✅ 已完成（基础设施）

1. **Generator 检测和执行**
   ```javascript
   const gen = myGenerator();
   __is_generator(gen);  // true
   ```

2. **Session 管理**
   ```javascript
   const session = __coroutine_session();  // 获取唯一会话ID
   ```

3. **协程等待/恢复机制**
   ```c
   // C 层可以：
   JS_CoroutineWait(mgr, ctx, generator, session, service_id);
   JS_CoroutineResume(mgr, session, response_data);
   ```

### 🚧 需要集成到 JTask（下一步）

要完全像 ltask 那样工作，还需要将协程系统集成到 jtask 中。

## 三、如何使用（集成后）

### 步骤 1：服务定义

```javascript
// service.js - 定义一个服务
function* myService(params) {
    console.log("服务启动，参数：", params);

    // 同步风格的异步调用！
    const user = yield jtask.call("user_service", {
        method: "getUser",
        id: params.userId
    });

    console.log("获取到用户：", user.name);

    // 可以连续调用
    const permissions = yield jtask.call("auth_service", {
        method: "getPermissions",
        userId: user.id
    });

    // 甚至可以并行调用（通过特殊语法）
    const [profile, settings] = yield jtask.parallel([
        jtask.call("profile_service", { userId: user.id }),
        jtask.call("settings_service", { userId: user.id })
    ]);

    return {
        user: user,
        permissions: permissions,
        profile: profile,
        settings: settings
    };
}
```

### 步骤 2：注册服务

```javascript
// 在 JTask 中注册服务
jtask.register("myService", myService);
```

### 步骤 3：调用服务

```javascript
// 从其他服务调用
function* anotherService() {
    const result = yield jtask.call("myService", { userId: 123 });
    console.log("完整用户信息：", result);
}
```

## 四、工作原理

### 1. 调用流程

```
Service A                    JTask Core                 Service B
    |                            |                          |
    | yield jtask.call("B")      |                          |
    |--------------------------->|                          |
    |                            | 生成 session=123        |
    |                            | 保存 generator A        |
    |                            | 发送消息到 B            |
    |                            |------------------------>|
    | (暂停在 yield)             |                          | 执行
    |                            |                          | return value
    |                            |<------------------------|
    |                            | 查找 session=123        |
    |                            | 恢复 generator A        |
    |<---------------------------|                          |
    | 继续执行                    |                          |
```

### 2. 核心机制

- **Session**: 每个 RPC 调用生成唯一 session ID
- **Generator 保存**: yield 时保存 generator 对象
- **自动恢复**: 收到响应后根据 session 自动恢复
- **线程安全**: 使用 mutex 保护跨线程访问

## 五、与 ltask 对比

| 特性 | ltask | 我们的实现 |
|-----|-------|----------|
| Generator 支持 | ✅ Lua 协程 | ✅ JS Generator |
| 同步风格 RPC | ✅ ltask.call | ✅ jtask.call |
| Session 管理 | ✅ 自动 | ✅ 自动 |
| 线程安全 | ✅ | ✅ mutex 保护 |
| 自动恢复 | ✅ | ✅ session 映射 |
| 错误处理 | ✅ | 🚧 需要完善 |
| 超时机制 | ✅ | 🚧 需要添加 |

## 六、立即可用的 API

### JavaScript 全局函数

```javascript
// 检查是否是 generator
__is_generator(obj)  // 返回 true/false

// 生成会话 ID
__coroutine_session()  // 返回唯一整数

// 等待协程（内部使用）
__coroutine_wait(generator, session, service_id)

// 恢复协程（内部使用）
__coroutine_resume(session, data)
```

### C API

```c
// 创建管理器
JSCoroutineManager* mgr = JS_NewCoroutineManager(rt);

// 启用协程
JS_EnableCoroutines(ctx, mgr);

// 生成会话
int session = JS_CoroutineGenerateSession(mgr);

// 等待/恢复
JS_CoroutineWait(mgr, ctx, generator, session, service_id);
JS_CoroutineResume(mgr, session, response_data);
```

## 七、编译和链接

### 编译协程扩展
```bash
cd /Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator
./build_coroutine.sh
```

### 在 JTask 中使用
```makefile
CFLAGS += -I/Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator
LIBS += /Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator/libquickjs_generator.a
LIBS += -lm -lpthread
```

## 八、下一步集成计划

### 需要修改 JTask 的部分：

1. **blueprinter.c**
   - 初始化协程管理器
   - 为每个 Context 启用协程

2. **服务创建**
   ```c
   struct service {
       JSContext *ctx;
       JSCoroutineManager *coroutine_mgr;
       JSValue generator;  // 如果是 generator 服务
   };
   ```

3. **消息处理**
   - 添加 session 字段到消息
   - yield 时保存 generator
   - 响应时恢复 generator

4. **JavaScript 层包装**
   ```javascript
   jtask.call = function(service, data) {
       return {
           __jtask_call__: true,
           target: service,
           data: data
       };
   };
   ```

## 九、完整示例

### 游戏场景应用

```javascript
// 玩家服务
function* playerService() {
    const playerId = yield jtask.call("auth", { token: "xxx" });
    const inventory = yield jtask.call("inventory", { playerId });
    const stats = yield jtask.call("stats", { playerId });

    return {
        id: playerId,
        inventory: inventory,
        stats: stats
    };
}

// 战斗服务
function* battleService(params) {
    // 获取双方玩家数据 - 看起来像同步！
    const player1 = yield jtask.call("player", { id: params.player1Id });
    const player2 = yield jtask.call("player", { id: params.player2Id });

    // 计算战斗结果
    const result = calculateBattle(player1, player2);

    // 更新双方状态 - 并行执行
    yield jtask.parallel([
        jtask.call("stats", {
            method: "update",
            playerId: player1.id,
            changes: result.player1Changes
        }),
        jtask.call("stats", {
            method: "update",
            playerId: player2.id,
            changes: result.player2Changes
        })
    ]);

    return result;
}
```

## 十、优势总结

1. **无阻塞**: Generator yield 不会阻塞线程
2. **高性能**: M:N 调度，充分利用多核
3. **易理解**: 代码看起来是同步的
4. **易调试**: 调用栈清晰，易于追踪
5. **无回调地狱**: 告别 callback 和 Promise 链

## 状态：核心已就绪，待集成到 JTask ✅

基础设施已经完成，现在需要将其集成到 JTask 中就能完全像 ltask 一样工作了！