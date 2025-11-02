# Tina 迁移计划：替换 quickjs_stackful_mini 中的 minicoro

**项目**: quickjs_stackful_mini  
**目标**: 用 Tina 协程库替换 minicoro  
**包含**: JTask 集成 + API 适配层  
**日期**: 2025-11-01  

---

## 📋 目录

1. [执行摘要](#执行摘要)
2. [当前架构分析](#当前架构分析)
3. [Minicoro API 映射到 Tina](#minicoro-api-映射到-tina)
4. [迁移策略](#迁移策略)
5. [JTask 集成方案](#jtask-集成方案)
6. [实施步骤](#实施步骤)
7. [测试计划](#测试计划)
8. [风险评估](#风险评估)
9. [回滚方案](#回滚方案)

---

## 📊 执行摘要

### 为什么要迁移？

| 对比维度 | Minicoro | Tina | 优势 |
|---------|----------|------|------|
| **代码质量** | 7.5/10 | 9.2/10 | Tina 更优 |
| **测试覆盖** | ⭐⭐ 无测试套件 | ⭐⭐⭐⭐⭐ 完整测试 | **Tina 胜** |
| **文档质量** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | **Tina 胜** |
| **架构支持** | x86_64/ARM64/ARM32 | x86/ARM/**RISC-V** | **Tina 胜** |
| **功能范围** | 纯协程 | 协程 + **作业调度** | **Tina 胜** |
| **可维护性** | 自修改代码 | Header-only | **Tina 胜** |
| **依赖管理** | ⭐⭐⭐⭐⭐ 零依赖 | ⭐⭐⭐⭐⭐ 零依赖 | 平局 |
| **构建系统** | Makefile | Makefile + CMake | **Tina 胜** |

### 关键收益

✅ **更高的代码质量**: Tina 评分 9.2/10 vs Minicoro 7.5/10  
✅ **完整测试保障**: Tina 有完整测试套件，Minicoro 无测试  
✅ **更好的可维护性**: 无自修改代码，Header-only 设计  
✅ **额外功能**: 内置作业调度系统（tina_jobs.h）  
✅ **更广平台支持**: 包括 RISC-V 和嵌入式系统  
✅ **对称/非对称协程**: 更灵活的编程模型  

### 迁移范围

**影响文件**:
- `/Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator/quickjs_stackful_mini.h` (62 行)
- `/Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator/quickjs_stackful_mini.c` (245 行)
- 测试文件：`test_mini_*.c` (4 个文件)
- **新增**: JTask 集成层 (约 300-500 行)

**工作量估算**: 2-3 周（包括完整测试）

---

## 🔍 当前架构分析

### Minicoro 使用概览

**集成方式**: Header-only 单头文件

```c
// 当前集成模式（quickjs_stackful_mini.c）
#define MINICORO_IMPL
#include "../minicoro/minicoro.h"
```

**核心数据结构**:

```c
// quickjs_stackful_mini.h
typedef struct {
    JSRuntime *rt;
    JSContext *main_ctx;
    /* Coroutine management */
    mco_coro **coroutines;    // minicoro 协程数组
    int cap;                  // 容量
    int count;                // 活跃协程数
    int running;              // 当前运行的协程 ID (-1 表示无)
} stackful_schedule;
```

### Minicoro API 使用清单

| API 函数 | 文件位置 | 行号 | 用途 |
|---------|---------|------|------|
| `mco_desc_init()` | quickjs_stackful_mini.c | 41 | 初始化协程描述符 |
| `mco_create()` | quickjs_stackful_mini.c | 46 | 创建协程 |
| `mco_resume()` | quickjs_stackful_mini.c | 67 | 恢复协程执行 |
| `mco_yield()` | quickjs_stackful_mini.c | 102 | 暂停协程 |
| `mco_destroy()` | quickjs_stackful_mini.c | 33, 73 | 销毁协程 |
| `mco_status()` | quickjs_stackful_mini.c | 65, 151 | 获取协程状态 |
| `mco_push()` | quickjs_stackful_mini.c | 114 | 推送数据到存储 |
| `mco_pop()` | quickjs_stackful_mini.c | 122 | 从存储弹出数据 |
| `mco_get_bytes_stored()` | quickjs_stackful_mini.c | 118 | 获取存储字节数 |
| `mco_get_user_data()` | test_mini_js.c | 77 | 获取用户数据 |

### 状态常量映射

```c
// Minicoro 状态
typedef enum mco_state {
  MCO_DEAD = 0,        // 已结束
  MCO_NORMAL = 1,      // 活跃但未运行
  MCO_RUNNING = 2,     // 正在执行
  MCO_SUSPENDED = 3    // 暂停（已 yield 或未启动）
} mco_state;

// 当前封装常量
#define STACKFUL_STATUS_DEAD MCO_DEAD
#define STACKFUL_STATUS_NORMAL MCO_NORMAL
#define STACKFUL_STATUS_RUNNING MCO_RUNNING
#define STACKFUL_STATUS_SUSPENDED MCO_SUSPENDED
```

### 配置参数

```c
// Minicoro 默认配置
#define MCO_MIN_STACK_SIZE 32768          // 32KB 最小栈
#define MCO_DEFAULT_STACK_SIZE 56*1024    // 56KB 默认栈
#define MCO_DEFAULT_STORAGE_SIZE 1024     // 1KB 数据存储

// 当前使用：默认栈大小（56KB）
mco_desc desc = mco_desc_init(func, 0);  // 0 = 使用默认
```

---

## 🔄 Minicoro API 映射到 Tina

### 核心 API 对照表

| Minicoro API | Tina 等价 API | 说明 |
|--------------|---------------|------|
| `mco_desc_init(func, stack_size)` | `tina_init(buffer, size, body, user_data)` | Tina 需要显式缓冲区或传 NULL |
| `mco_create(&coro, &desc)` | `tina* t = tina_init(...)` | Tina 直接返回指针 |
| `mco_resume(coro)` | `tina_resume(coro, value)` | Tina 返回 yield 的值 |
| `mco_yield(coro)` | `tina_yield(coro, value)` | 非对称 yield |
| `mco_destroy(coro)` | `free(coro->buffer)` (如果 malloc 分配) | Tina 手动管理内存 |
| `mco_status(coro)` | `coro->completed` 布尔值 | Tina 只区分完成/未完成 |
| `mco_push(coro, data, len)` | **需自定义实现** | Tina 无内置存储 |
| `mco_pop(coro, data, len)` | **需自定义实现** | Tina 无内置存储 |
| `mco_get_user_data(coro)` | `coro->user_data` | 直接访问字段 |
| `mco_running()` | **需自定义实现** (thread_local) | Tina 无全局状态 |

### 状态映射策略

**Minicoro 4 状态 → Tina 扩展状态**

```c
// Tina 原生状态
typedef struct tina {
    bool completed;              // 是否已完成
    // ... 其他字段
} tina;

// 迁移策略：增强 stackful_schedule 跟踪状态
typedef enum {
    TINA_STATUS_DEAD = 0,       // 对应 MCO_DEAD
    TINA_STATUS_NORMAL = 1,     // 对应 MCO_NORMAL（准备恢复）
    TINA_STATUS_RUNNING = 2,    // 对应 MCO_RUNNING（正在执行）
    TINA_STATUS_SUSPENDED = 3   // 对应 MCO_SUSPENDED（已 yield）
} tina_status_ext;

typedef struct {
    tina *coro;
    tina_status_ext status;
    int yield_count;            // 用于区分未启动/已 yield
} tina_wrapper;
```

### 数据存储层实现

Tina 没有内置 push/pop 存储，需要自定义：

```c
// 新增：数据存储结构
typedef struct {
    uint8_t buffer[1024];       // 固定 1KB 缓冲区
    size_t size;                // 当前存储字节数
} tina_storage;

// 在 stackful_schedule 中添加
typedef struct {
    // ... 现有字段 ...
    tina_storage *storages;     // 每协程一个存储
} stackful_schedule;

// 实现 push/pop
int tina_storage_push(tina_storage *s, const void *data, size_t len) {
    if (s->size + len > sizeof(s->buffer)) {
        return -1;  // 缓冲区满
    }
    memcpy(s->buffer + s->size, data, len);
    s->size += len;
    return 0;
}

int tina_storage_pop(tina_storage *s, void *data, size_t len) {
    if (s->size < len) {
        return -1;  // 数据不足
    }
    memcpy(data, s->buffer + s->size - len, len);
    s->size -= len;
    return 0;
}
```

### 栈大小配置

```c
// Minicoro：动态或默认
mco_desc desc = mco_desc_init(func, 56*1024);

// Tina：需要预分配或 NULL 自动分配
#define TINA_DEFAULT_STACK_SIZE (56*1024)

tina *coro = tina_init(NULL, TINA_DEFAULT_STACK_SIZE, 
                       func, user_data);
// NULL → tina 内部 malloc
// 需手动 free(coro->buffer) + free(coro)
```

---

## 🎯 迁移策略

### 策略 A：最小化改动（推荐）

**原则**: 保持现有 `stackful_*` API 不变，仅替换底层实现

**优势**:
- ✅ 对外 API 100% 兼容
- ✅ 测试代码无需修改
- ✅ QuickJS 集成层无需修改
- ✅ 风险最小

**实施**:

```c
// quickjs_stackful_mini.h - 无需修改接口
int stackful_new(stackful_schedule *S, void (*func)(mco_coro*), void *ud);
int stackful_resume(stackful_schedule *S, int id);
void stackful_yield(stackful_schedule *S);
// ... 其他函数保持不变

// quickjs_stackful_mini.c - 修改实现
// 替换：#include "../minicoro/minicoro.h"
// 为：  #include "../Tina/tina.h"

typedef struct {
    JSRuntime *rt;
    JSContext *main_ctx;
    /* Coroutine management */
    tina **coroutines;          // 改为 tina 指针数组
    tina_status_ext *statuses;  // 新增状态跟踪
    tina_storage *storages;     // 新增数据存储
    int cap;
    int count;
    int running;
} stackful_schedule;
```

### 策略 B：增强型迁移

**原则**: 利用 Tina 的对称协程和作业调度功能

**优势**:
- ✅ 利用 Tina 作业系统（tina_jobs.h）
- ✅ 支持对称协程（灵活切换）
- ✅ 更高性能

**劣势**:
- ⚠️ API 需要修改
- ⚠️ 测试需要更新
- ⚠️ 工作量更大

**保留用于 Phase 2**

---

## 🔗 JTask 集成方案

### JTask 当前架构概览

```
JTask 服务模型：
┌─────────────────────────────────────┐
│  Service A (独立 VM)                 │
│  ├─ mainloop_coro_id (协程 0)       │
│  ├─ fork coroutines (协程 1,2,3...) │
│  ├─ waiting_coroutine_id            │
│  └─ wakeupQueue (蹦床队列)          │
└─────────────────────────────────────┘
         ↕ (消息传递)
┌─────────────────────────────────────┐
│  C 运行时层 (jtask.c, service.c)    │
│  ├─ 工作线程池                       │
│  ├─ 调度器                           │
│  ├─ 消息队列                         │
│  └─ Receipt 机制                     │
└─────────────────────────────────────┘
```

### JTask 协程 API

**当前使用 minicoro** (需要蹦床模式):

```javascript
// jslib/service.js + jtask_api.c
jtask.create_coroutine(fn)           // 创建协程
jtask.resume_coroutine(coro_id)      // 恢复协程
jtask.yield_control()                // Yield 到 C 层
jtask.coroutine_status(coro_id)      // 获取状态
jtask.running_coroutine()            // 获取当前协程 ID

// 蹦床模式（因为 minicoro 限制）
while (wakeupQueue.length > 0) {
    let coro_id = wakeupQueue.shift();
    stackful_resume(coro_id);  // 必须从主线程调用
}
```

### Tina 集成到 JTask 的三种方案

#### 方案 1：透明替换（最小改动）

**原理**: quickjs_stackful_mini 内部用 Tina，JTask 无感知

```
JTask (jtask.c)
  ↓ 调用
stackful_resume(coro_id)
  ↓ 内部实现
tina_resume(tina_coros[coro_id], value)
```

**优势**:
- ✅ JTask 代码零改动
- ✅ 测试用例零改动
- ✅ 最快实施

**劣势**:
- ⚠️ 仍需蹦床模式（因为 API 兼容）
- ⚠️ 未利用 Tina 对称协程优势

**推荐度**: ⭐⭐⭐⭐⭐ (Phase 1 采用)

#### 方案 2：原生 Tina 集成（深度优化）

**原理**: JTask 直接使用 Tina API，移除蹦床

```c
// 新增：src/jtask_tina.c (Tina 专用绑定层)
JSValue js_tina_create(JSContext *ctx, ...);
JSValue js_tina_resume(JSContext *ctx, ...);
JSValue js_tina_yield(JSContext *ctx, ...);

// 修改：jslib/service.js
// 移除 wakeupQueue
// 直接使用对称协程切换
```

**优势**:
- ✅ 性能最优（无蹦床开销）
- ✅ 代码更简洁
- ✅ 支持对称协程

**劣势**:
- ⚠️ JTask 大量修改
- ⚠️ 测试全部重写
- ⚠️ 风险高

**推荐度**: ⭐⭐⭐ (Phase 2 可选)

#### 方案 3：Tina 作业调度器（最大增强）

**原理**: 利用 tina_jobs.h 替代 JTask 调度器

```c
// 使用 Tina 的作业系统
#include "tina_jobs.h"

tina_scheduler *sched = tina_scheduler_new(...);

// 每个 JTask service → 一个 Tina job
tina_job job = {
    .func = service_entry_point,
    .data = service_ptr,
};
tina_scheduler_add(sched, &job);
```

**优势**:
- ✅ 工作窃取调度（更高吞吐）
- ✅ 优先级队列支持
- ✅ 内置性能优化

**劣势**:
- ⚠️ 架构完全重写
- ⚠️ 工作量巨大（4-6 周）
- ⚠️ 高风险

**推荐度**: ⭐ (Phase 3 研究型项目)

### 选定方案：方案 1（透明替换）

**理由**:
1. 最小风险
2. 最快交付
3. 保留未来升级路径
4. JTask 当前稳定，无需重构

---

## 📝 实施步骤

### Phase 1: 准备工作（1-2 天）

#### 1.1 获取 Tina 源码

```bash
cd /Volumes/thunderbolt/works/11/mo/3rd/
git clone https://github.com/slembcke/Tina.git
# 或者使用已有的 Tina 副本
```

#### 1.2 分析 Tina API

**阅读文件**:
- `Tina/tina.h` - 核心协程 API
- `Tina/tina_jobs.h` - 作业调度（可选）
- `Tina/README.md` - 文档
- `Tina/extras/examples/` - 示例代码

**关键检查**:
- 栈大小配置方式
- 内存分配模式
- 平台支持（确认 macOS/x86_64）
- 线程安全性

#### 1.3 创建测试分支

```bash
cd /Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator
git checkout -b feature/tina-migration
```

#### 1.4 备份当前实现

```bash
cp quickjs_stackful_mini.h quickjs_stackful_mini.h.minicoro.bak
cp quickjs_stackful_mini.c quickjs_stackful_mini.c.minicoro.bak
```

---

### Phase 2: 核心迁移（3-4 天）

#### 2.1 修改头文件（quickjs_stackful_mini.h）

```c
// 替换 include
// #include "../minicoro/minicoro.h"
#include "../Tina/tina.h"

// 添加扩展状态枚举
typedef enum {
    TINA_STATUS_DEAD = 0,
    TINA_STATUS_NORMAL = 1,
    TINA_STATUS_RUNNING = 2,
    TINA_STATUS_SUSPENDED = 3
} tina_status_ext;

// 添加数据存储结构
typedef struct {
    uint8_t buffer[1024];
    size_t size;
} tina_storage;

// 修改调度器结构
typedef struct {
    JSRuntime *rt;
    JSContext *main_ctx;
    /* Coroutine management */
    tina **coroutines;          // 改为 tina**
    tina_status_ext *statuses;  // 新增：状态数组
    tina_storage *storages;     // 新增：存储数组
    int cap;
    int count;
    int running;
} stackful_schedule;

// API 保持不变
stackful_schedule* stackful_open(JSRuntime *rt, JSContext *main_ctx);
void stackful_close(stackful_schedule *S);
int stackful_new(stackful_schedule *S, void (*func)(mco_coro*), void *ud);
int stackful_resume(stackful_schedule *S, int id);
void stackful_yield(stackful_schedule *S);
// ... 其他函数
```

#### 2.2 实现核心函数（quickjs_stackful_mini.c）

**2.2.1 初始化/销毁**

```c
#define TINA_IMPLEMENTATION
#include "quickjs_stackful_mini.h"

#define DEFAULT_COROUTINE 16
#define TINA_DEFAULT_STACK_SIZE (56*1024)  // 匹配 minicoro

stackful_schedule* stackful_open(JSRuntime *rt, JSContext *main_ctx) {
    stackful_schedule *S = malloc(sizeof(*S));
    S->rt = rt;
    S->main_ctx = main_ctx;
    S->cap = DEFAULT_COROUTINE;
    S->count = 0;
    S->running = -1;
    
    // 分配三个数组
    S->coroutines = calloc(S->cap, sizeof(tina*));
    S->statuses = calloc(S->cap, sizeof(tina_status_ext));
    S->storages = calloc(S->cap, sizeof(tina_storage));
    
    return S;
}

void stackful_close(stackful_schedule *S) {
    for (int i = 0; i < S->cap; i++) {
        if (S->coroutines[i]) {
            free(S->coroutines[i]->buffer);
            // Tina 可能需要 free(S->coroutines[i]) 或者已由 buffer 包含
        }
    }
    free(S->coroutines);
    free(S->statuses);
    free(S->storages);
    free(S);
}
```

**2.2.2 协程创建**

```c
// Tina 协程入口适配器
static void* tina_entry_wrapper(tina *coro, void *value) {
    void (*func)(mco_coro*) = (void(*)(mco_coro*))coro->user_data;
    
    // 调用原始函数（参数类型兼容，因为都是 void* 上下文）
    func((mco_coro*)coro);
    
    return NULL;  // Tina 要求返回 void*
}

int stackful_new(stackful_schedule *S, void (*func)(mco_coro*), void *ud) {
    // 查找空闲槽位
    int id = -1;
    for (int i = 0; i < S->cap; i++) {
        if (S->coroutines[i] == NULL) {
            id = i;
            break;
        }
    }
    
    // 扩容
    if (id == -1) {
        id = S->cap;
        S->cap *= 2;
        S->coroutines = realloc(S->coroutines, S->cap * sizeof(tina*));
        S->statuses = realloc(S->statuses, S->cap * sizeof(tina_status_ext));
        S->storages = realloc(S->storages, S->cap * sizeof(tina_storage));
        
        memset(S->coroutines + id, 0, (S->cap - id) * sizeof(tina*));
        memset(S->statuses + id, 0, (S->cap - id) * sizeof(tina_status_ext));
        memset(S->storages + id, 0, (S->cap - id) * sizeof(tina_storage));
    }
    
    // 创建 Tina 协程
    S->coroutines[id] = tina_init(NULL, TINA_DEFAULT_STACK_SIZE, 
                                   tina_entry_wrapper, func);
    if (!S->coroutines[id]) {
        return -1;
    }
    
    // 保存用户数据到 Tina（如果需要）
    // 注意：func 已经作为 user_data，ud 需要另外存储
    // 这里需要设计一个包装结构
    
    S->statuses[id] = TINA_STATUS_SUSPENDED;  // 未启动
    S->count++;
    
    return id;
}
```

**问题**: `ud` (user_data) 传递问题

**解决方案**:

```c
// 定义包装结构
typedef struct {
    void (*func)(mco_coro*);
    void *user_data;
    stackful_schedule *schedule;
    int coro_id;
} tina_context;

// 修改 stackful_new
int stackful_new(stackful_schedule *S, void (*func)(mco_coro*), void *ud) {
    // ... 查找 id 逻辑 ...
    
    // 创建上下文
    tina_context *ctx = malloc(sizeof(tina_context));
    ctx->func = func;
    ctx->user_data = ud;
    ctx->schedule = S;
    ctx->coro_id = id;
    
    // 创建协程
    S->coroutines[id] = tina_init(NULL, TINA_DEFAULT_STACK_SIZE, 
                                   tina_entry_wrapper, ctx);
    // ... rest ...
}

// 修改 wrapper
static void* tina_entry_wrapper(tina *coro, void *value) {
    tina_context *ctx = (tina_context*)coro->user_data;
    ctx->func((mco_coro*)coro);  // 调用原函数
    free(ctx);  // 清理
    return NULL;
}
```

**2.2.3 协程恢复**

```c
int stackful_resume(stackful_schedule *S, int id) {
    if (id < 0 || id >= S->cap || S->coroutines[id] == NULL) {
        return -1;
    }
    
    tina *coro = S->coroutines[id];
    int caller_id = S->running;
    S->running = id;
    
    // 更新状态
    S->statuses[id] = TINA_STATUS_RUNNING;
    
    // 恢复协程
    void *result = tina_resume(coro, NULL);
    
    // 检查完成状态
    if (coro->completed) {
        S->statuses[id] = TINA_STATUS_DEAD;
        
        // 清理
        free(coro->buffer);
        S->coroutines[id] = NULL;
        S->count--;
    } else {
        S->statuses[id] = TINA_STATUS_SUSPENDED;
    }
    
    S->running = caller_id;
    return 0;
}
```

**2.2.4 协程 Yield**

```c
void stackful_yield(stackful_schedule *S) {
    int id = S->running;
    if (id < 0 || id >= S->cap || S->coroutines[id] == NULL) {
        return;
    }
    
    tina *coro = S->coroutines[id];
    
    // Tina yield（需要从协程内部调用）
    tina_yield(coro, NULL);
    
    // 状态由 resume 更新
}
```

**2.2.5 状态查询**

```c
mco_state stackful_status(stackful_schedule *S, int id) {
    if (id < 0 || id >= S->cap || S->coroutines[id] == NULL) {
        return TINA_STATUS_DEAD;
    }
    return S->statuses[id];
}

int stackful_running(stackful_schedule *S) {
    return S->running;
}
```

**2.2.6 数据存储函数**

```c
void stackful_yield_with_flag(stackful_schedule *S, int flag) {
    int id = S->running;
    if (id < 0 || id >= S->cap) {
        return;
    }
    
    tina_storage *storage = &S->storages[id];
    
    // 推送 flag
    if (tina_storage_push(storage, &flag, sizeof(int)) < 0) {
        // 错误处理
        return;
    }
    
    stackful_yield(S);
}

int stackful_pop_continue_flag(stackful_schedule *S) {
    int id = S->running;
    if (id < 0 || id >= S->cap) {
        return 0;
    }
    
    tina_storage *storage = &S->storages[id];
    
    int flag = 0;
    if (tina_storage_pop(storage, &flag, sizeof(int)) < 0) {
        return 0;
    }
    
    return flag;
}

// 实现存储辅助函数
int tina_storage_push(tina_storage *s, const void *data, size_t len) {
    if (s->size + len > sizeof(s->buffer)) {
        return -1;
    }
    memcpy(s->buffer + s->size, data, len);
    s->size += len;
    return 0;
}

int tina_storage_pop(tina_storage *s, void *data, size_t len) {
    if (s->size < len) {
        return -1;
    }
    s->size -= len;
    memcpy(data, s->buffer + s->size, len);
    return 0;
}

size_t tina_storage_bytes(tina_storage *s) {
    return s->size;
}
```

#### 2.3 处理 QuickJS 集成层

**检查点**: `stackful_enable_js_api` 函数

```c
// 这个函数应该无需修改，因为它只调用 stackful_* API
int stackful_enable_js_api(JSContext *ctx, stackful_schedule *S) {
    g_stackful_schedule = S;
    
    // ... 注册 JS 函数 ...
    // 所有函数都调用 stackful_* API，已被适配
    
    return 0;
}
```

#### 2.4 编译测试

```bash
cd /Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator

# 编译简单测试
gcc -o test_mini_simple_tina test_mini_simple.c \
    quickjs_stackful_mini.c \
    -I. -I../Tina \
    -DTINA_IMPLEMENTATION \
    -lm -lpthread

# 运行
./test_mini_simple_tina
```

---

### Phase 3: 测试验证（2-3 天）

#### 3.1 单元测试

**测试 1: 简单协程** (`test_mini_simple.c`)

```bash
gcc -o test_mini_simple_tina test_mini_simple.c \
    quickjs_stackful_mini.c \
    -I. -I../Tina -lm -lpthread
    
./test_mini_simple_tina

# 期望输出：
# [Scheduler] Created
# [Coro 0] First yield
# [Main] Resumed coro 0
# [Coro 0] Second yield
# [Main] Resumed coro 0 again
# [Coro 0] Exiting
# [Main] Coro 0 status: 0 (dead)
```

**测试 2: QuickJS 集成** (`test_mini_js.c`)

```bash
gcc -o test_mini_js_tina test_mini_js.c \
    quickjs_stackful_mini.c \
    quickjs.o quickjs-libc.o cutils.o dtoa.o libunicode.o libregexp.o \
    -I. -I../Tina -lm -lpthread

./test_mini_js_tina

# 期望输出：
# [mock_call] 被调用
# [mock_call] Yielding...
# [C] Coroutine yielded, resuming...
# [mock_call] Resumed!
# Result: result from mock_call
```

**测试 3: 高级功能** (`test_mini_advanced.c`)

```bash
gcc -o test_mini_advanced_tina test_mini_advanced.c \
    quickjs_stackful_mini.c \
    quickjs.o quickjs-libc.o cutils.o dtoa.o libunicode.o libregexp.o \
    -I. -I../Tina -lm -lpthread

./test_mini_advanced_tina

# 验证：
# - 循环中的 yield
# - 嵌套函数调用
# - 回调函数
# - 箭头函数
```

**测试 4: 数据传递** (`test_yield_data.c`)

```bash
gcc -o test_yield_data_tina test_yield_data.c \
    quickjs_stackful_mini.c \
    -I. -I../Tina -lm -lpthread

./test_yield_data_tina

# 验证 push/pop 数据传递
```

#### 3.2 性能对比测试

**创建基准测试**: `benchmark_tina.c`

```c
#include "quickjs_stackful_mini.h"
#include <time.h>

void benchmark_coro(mco_coro *co) {
    for (int i = 0; i < 1000; i++) {
        stackful_yield(stackful_get_global_schedule());
    }
}

int main() {
    stackful_schedule *S = stackful_open(NULL, NULL);
    
    clock_t start = clock();
    
    int coro_id = stackful_new(S, benchmark_coro, NULL);
    
    for (int i = 0; i < 1000; i++) {
        stackful_resume(S, coro_id);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("1000 resume/yield cycles: %.6f seconds\n", elapsed);
    printf("Average per cycle: %.2f μs\n", elapsed * 1000000 / 1000);
    
    stackful_close(S);
    return 0;
}
```

**运行对比**:

```bash
# Minicoro 版本
gcc -o bench_minicoro benchmark_tina.c \
    quickjs_stackful_mini.c.minicoro.bak \
    -I. -I../minicoro -lm -lpthread
./bench_minicoro

# Tina 版本
gcc -o bench_tina benchmark_tina.c \
    quickjs_stackful_mini.c \
    -I. -I../Tina -lm -lpthread
./bench_tina

# 比较结果
```

#### 3.3 内存泄漏检测

```bash
# 使用 Valgrind (Linux) 或 Instruments (macOS)
valgrind --leak-check=full ./test_mini_simple_tina

# macOS 使用 leaks
leaks --atExit -- ./test_mini_simple_tina
```

---

### Phase 4: JTask 集成测试（2-3 天）

#### 4.1 复制 quickjs_stackful_mini 到 JTask

```bash
cp quickjs_stackful_mini.h \
   /Volumes/thunderbolt/works/11/mo/3rd/jtask/src/
cp quickjs_stackful_mini.c \
   /Volumes/thunderbolt/works/11/mo/3rd/jtask/src/

# 复制 Tina
cp -r ../Tina /Volumes/thunderbolt/works/11/mo/3rd/jtask/deps/
```

#### 4.2 修改 JTask 构建系统

**修改**: `tools/buildosx.c` (macOS) 或 `tools/buildwin.c` (Windows)

```c
// 添加 Tina include 路径
const char *tina_include = "-Ideps/Tina";

// 添加到编译命令
nob_cmd_append(&cmd, tina_include);

// 添加 quickjs_stackful_mini.c 到源文件列表
nob_cmd_append(&cmd, "src/quickjs_stackful_mini.c");
```

#### 4.3 运行 JTask 测试

```bash
cd /Volumes/thunderbolt/works/11/mo/3rd/jtask

# 重新构建
make clean
make

# 运行测试
./build/jtask test/start.js

# 检查日志
tail -f logs/jtask.log
```

**验证点**:
- ✅ Root 服务（ID=1）创建成功
- ✅ Timer 服务（ID=2）初始化成功
- ✅ 协程创建和恢复正常
- ✅ 消息传递无错误
- ✅ Receipt 机制正常工作
- ✅ 无内存泄漏

#### 4.4 压力测试

```javascript
// test/stress_coro.js
const jtask = require('jtask');

// 创建 100 个协程
for (let i = 0; i < 100; i++) {
    jtask.create_coroutine(function() {
        for (let j = 0; j < 10; j++) {
            jtask.yield_control();
        }
    });
}

console.log('Created 100 coroutines with 10 yields each');
```

```bash
./build/jtask test/stress_coro.js
```

---

### Phase 5: 文档和清理（1 天）

#### 5.1 更新文档

**更新文件**:

1. `STACKFUL_README.md`:
   ```markdown
   ## 协程实现
   
   本项目使用 [Tina](https://github.com/slembcke/Tina) 作为协程库。
   
   ### 为什么选择 Tina？
   
   - ✅ 更高的代码质量（9.2/10 vs minicoro 7.5/10）
   - ✅ 完整的测试套件
   - ✅ 更好的文档
   - ✅ 支持更多平台（包括 RISC-V）
   - ✅ 可选的作业调度系统
   
   ### 配置
   
   默认栈大小：56KB（与 minicoro 一致）
   数据存储：1KB per 协程
   ```

2. `JTASK_STACKFUL_INTEGRATION.md`:
   ```markdown
   ## 协程库变更
   
   ### v2.0 (2025-11-01)
   
   - 迁移到 Tina 协程库
   - 保持 API 100% 兼容
   - 性能提升：[基准测试结果]
   - 内存优化：[对比数据]
   ```

3. 新增 `TINA_MIGRATION_NOTES.md`:
   ```markdown
   # Tina 迁移说明
   
   ## API 映射
   
   | Minicoro | Tina |
   |----------|------|
   | mco_create | tina_init |
   | mco_resume | tina_resume |
   | ... | ... |
   
   ## 已知差异
   
   1. 状态跟踪：Tina 仅区分完成/未完成，扩展状态在 stackful_schedule 中维护
   2. 数据存储：自定义实现 tina_storage
   3. 内存管理：Tina 需要手动 free buffer
   ```

#### 5.2 清理代码

```bash
# 移除备份文件（如果测试通过）
rm *.bak

# 移除 minicoro 依赖
rm -rf ../minicoro  # 或移到归档目录

# 更新 .gitignore
echo "*.bak" >> .gitignore
```

#### 5.3 提交代码

```bash
git add -A
git commit -m "迁移协程库：Minicoro → Tina

- 替换 minicoro 为 Tina
- 保持 stackful_* API 100% 兼容
- 新增扩展状态跟踪和数据存储
- 通过所有测试用例
- 性能对比：[结果]

详见：TINA_MIGRATION_PLAN.md"

git push origin feature/tina-migration
```

---

## 🧪 测试计划

### 测试矩阵

| 测试类型 | 测试文件 | 验证内容 | 状态 |
|---------|---------|---------|------|
| **单元测试** |
| 简单协程 | test_mini_simple.c | 创建、恢复、yield、销毁 | ⬜️ |
| QuickJS 集成 | test_mini_js.c | JS API、全局对象、mock_call | ⬜️ |
| 高级功能 | test_mini_advanced.c | 循环、嵌套、回调、箭头函数 | ⬜️ |
| 数据传递 | test_yield_data.c | push/pop 数据存储 | ⬜️ |
| **性能测试** |
| 基准测试 | benchmark_tina.c | 1000 次 resume/yield 时间 | ⬜️ |
| 内存测试 | (Valgrind) | 内存泄漏检测 | ⬜️ |
| **集成测试** |
| JTask 启动 | test/start.js | Root 服务创建 | ⬜️ |
| 服务通信 | test/service.js | 消息传递、receipt | ⬜️ |
| 协程压力 | test/stress_coro.js | 100+ 协程并发 | ⬜️ |
| **回归测试** |
| 所有现有测试 | test/*.js | 确保无破坏 | ⬜️ |

### 验收标准

**必须通过**:
- ✅ 所有现有测试用例无修改通过
- ✅ 无内存泄漏（Valgrind/leaks 检测）
- ✅ 性能不低于 minicoro（±10% 可接受）
- ✅ JTask 运行稳定（无崩溃、无死锁）

**可选通过**:
- 🎯 性能提升 >10%
- 🎯 内存占用降低
- 🎯 支持新平台（如 RISC-V）

---

## ⚠️ 风险评估

### 高风险项

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|-------|------|---------|
| **API 语义差异** | 中 | 高 | 详细的 API 映射文档，逐个函数验证 |
| **内存管理错误** | 中 | 高 | Valgrind 检测，代码审查 |
| **性能退化** | 低 | 中 | 基准测试，性能监控 |
| **JTask 集成失败** | 低 | 高 | 增量集成，保留 minicoro 回滚路径 |

### 中风险项

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|-------|------|---------|
| **数据存储功能不完整** | 中 | 中 | 自定义实现 tina_storage |
| **状态跟踪不准确** | 低 | 中 | 扩展 stackful_schedule 结构 |
| **编译问题** | 低 | 低 | CMake/Makefile 双构建系统 |

### 低风险项

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|-------|------|---------|
| **文档不同步** | 中 | 低 | 迁移检查清单 |
| **测试覆盖不全** | 低 | 低 | 测试矩阵 |

---

## 🔙 回滚方案

### 快速回滚（< 5 分钟）

```bash
# 恢复 minicoro 版本
git checkout main
git branch -D feature/tina-migration

# 或者使用备份
cp quickjs_stackful_mini.h.minicoro.bak quickjs_stackful_mini.h
cp quickjs_stackful_mini.c.minicoro.bak quickjs_stackful_mini.c

# 重新编译
make clean && make
```

### 部分回滚（JTask 保持 minicoro）

```bash
# 仅在 quickjs_generator 使用 Tina
# JTask 继续使用 minicoro

# 方法：保留两套实现
# - quickjs_stackful_mini_tina.c (Tina 版本)
# - quickjs_stackful_mini_mco.c (minicoro 版本)
# JTask 根据配置选择
```

### 迁移状态检查点

| 阶段 | 检查点 | 如果失败 |
|------|-------|---------|
| Phase 2.2 | 编译成功 | 检查 Tina API 调用，修复语法错误 |
| Phase 3.1 | 单元测试通过 | 逐个调试失败用例，必要时回滚 |
| Phase 3.2 | 性能可接受 | 优化或接受差异，文档说明 |
| Phase 4.3 | JTask 基础功能正常 | 回滚 JTask 集成，quickjs_generator 继续 |
| Phase 4.4 | 压力测试通过 | 修复或限制并发数量 |

---

## 📈 成功指标

### 量化指标

| 指标 | 目标 | 测量方法 |
|------|------|---------|
| **测试通过率** | 100% | 所有测试用例 |
| **性能对比** | ±10% | benchmark_tina.c |
| **内存泄漏** | 0 | Valgrind/leaks |
| **代码行数** | <300 行改动 | git diff --stat |
| **编译时间** | ±5% | time make |
| **JTask 稳定性** | 24h 无崩溃 | 长期运行测试 |

### 定性指标

- ✅ 代码可读性提升
- ✅ 文档完整性
- ✅ 团队对新库的熟悉度
- ✅ 未来可扩展性（如对称协程）

---

## 📚 参考资源

### Tina 文档

- **GitHub**: https://github.com/slembcke/Tina
- **主头文件**: `/Volumes/thunderbolt/works/11/mo/4rd/Tina/tina.h`
- **作业调度**: `/Volumes/thunderbolt/works/11/mo/4rd/Tina/tina_jobs.h`
- **README**: `/Volumes/thunderbolt/works/11/mo/4rd/Tina/README.md`
- **示例**: `/Volumes/thunderbolt/works/11/mo/4rd/Tina/extras/examples/`

### Minicoro 文档

- **GitHub**: https://github.com/edubart/minicoro
- **当前头文件**: `/Volumes/thunderbolt/works/11/mo/3rd/minicoro/minicoro.h`

### 项目文档

- **QuickJS Stackful**: `/Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator/STACKFUL_README.md`
- **JTask 集成**: `/Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator/JTASK_STACKFUL_INTEGRATION.md`
- **魔法原理**: `/Volumes/thunderbolt/works/11/mo/3rd/quickjs_generator/STACKFUL_MAGIC.md`

### JTask 项目

- **项目路径**: `/Volumes/thunderbolt/works/11/mo/3rd/jtask`
- **工作日志**: `/Volumes/thunderbolt/works/11/mo/3rd/jtask/WORK_LOG.md`
- **架构文档**: `/Volumes/thunderbolt/works/11/mo/3rd/jtask/docs/project/`

---

## ✅ 迁移检查清单

### 准备阶段
- [ ] 阅读 Tina README 和示例代码
- [ ] 创建 feature/tina-migration 分支
- [ ] 备份现有实现 (*.bak)
- [ ] 准备测试环境

### 实施阶段
- [ ] 修改 quickjs_stackful_mini.h（头文件）
- [ ] 实现 stackful_open/close
- [ ] 实现 stackful_new（协程创建）
- [ ] 实现 stackful_resume（协程恢复）
- [ ] 实现 stackful_yield（协程暂停）
- [ ] 实现 stackful_status/running（状态查询）
- [ ] 实现 tina_storage（数据存储）
- [ ] 实现 stackful_yield_with_flag/pop_continue_flag

### 测试阶段
- [ ] 编译通过（无警告）
- [ ] test_mini_simple 通过
- [ ] test_mini_js 通过
- [ ] test_mini_advanced 通过
- [ ] test_yield_data 通过
- [ ] 基准测试（性能对比）
- [ ] 内存泄漏检测（Valgrind/leaks）

### JTask 集成阶段
- [ ] 复制文件到 jtask/src
- [ ] 修改构建系统（buildosx/buildwin）
- [ ] JTask 编译通过
- [ ] Root 服务创建成功
- [ ] Timer 服务初始化成功
- [ ] 消息传递正常
- [ ] Receipt 机制正常
- [ ] 压力测试通过（100+ 协程）

### 文档阶段
- [ ] 更新 STACKFUL_README.md
- [ ] 更新 JTASK_STACKFUL_INTEGRATION.md
- [ ] 创建 TINA_MIGRATION_NOTES.md
- [ ] 更新代码注释
- [ ] 提交 Git commit

### 验收阶段
- [ ] 所有测试通过
- [ ] 性能可接受（±10%）
- [ ] 无内存泄漏
- [ ] JTask 稳定运行 24h
- [ ] 代码审查通过
- [ ] 合并到主分支

---

## 📞 支持联系

**问题追踪**: 在实施过程中遇到问题，记录在本文档末尾的"问题日志"部分

**代码审查**: 请至少一位熟悉协程和 QuickJS 的开发者审查关键代码

---

## 📝 附录 A：API 快速参考

### Minicoro → Tina 快速映射

```c
// 创建协程
mco_desc desc = mco_desc_init(func, stack_size);
mco_result res = mco_create(&coro, &desc);
// →
tina *coro = tina_init(NULL, stack_size, func, user_data);

// 恢复协程
mco_result res = mco_resume(coro);
// →
void *result = tina_resume(coro, value);

// Yield
mco_result res = mco_yield(coro);
// →
void *result = tina_yield(coro, value);

// 状态查询
mco_state state = mco_status(coro);
// →
bool completed = coro->completed;
// + 自定义状态跟踪

// 销毁
mco_destroy(coro);
// →
free(coro->buffer);  // 如果 buffer 由 tina 分配

// 用户数据
void *ud = mco_get_user_data(coro);
// →
void *ud = coro->user_data;
```

### Tina 特有功能

```c
// 对称协程（直接切换）
void *result = tina_swap(from_coro, to_coro, value);

// 作业调度器（可选使用）
#include "tina_jobs.h"
tina_scheduler *sched = tina_scheduler_new(...);
tina_scheduler_add(sched, &job);
tina_scheduler_run(sched, ...);
```

---

## 📝 附录 B：故障排查指南

### 编译错误

**问题**: `undefined reference to 'tina_init'`

**解决**:
```c
// 确保定义了 TINA_IMPLEMENTATION
#define TINA_IMPLEMENTATION
#include "tina.h"
```

**问题**: `tina.h: No such file or directory`

**解决**:
```bash
# 检查 include 路径
gcc ... -I../Tina
# 或者使用绝对路径
gcc ... -I/Volumes/thunderbolt/works/11/mo/4rd/Tina
```

### 运行时错误

**问题**: Segmentation fault 在 tina_resume

**调试**:
```bash
# 使用 gdb 或 lldb
lldb ./test_mini_simple_tina
(lldb) run
(lldb) bt  # 查看堆栈跟踪
(lldb) frame select 0
(lldb) print *coro
```

**常见原因**:
- 栈大小不足（增加 TINA_DEFAULT_STACK_SIZE）
- 协程已销毁但仍被访问（检查生命周期）
- user_data 未正确传递

**问题**: 协程状态不正确

**解决**:
```c
// 添加调试日志
printf("[DEBUG] Coro %d status: %d, completed: %d\n", 
       id, S->statuses[id], S->coroutines[id]->completed);
```

### 性能问题

**问题**: Tina 版本比 minicoro 慢 >20%

**诊断**:
```c
// 添加性能计数器
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);
tina_resume(coro, NULL);
clock_gettime(CLOCK_MONOTONIC, &end);
long ns = (end.tv_sec - start.tv_sec) * 1000000000L + 
          (end.tv_nsec - start.tv_nsec);
printf("Resume time: %ld ns\n", ns);
```

**可能原因**:
- 栈内存未对齐（Tina 要求对齐）
- 过多的状态跟踪开销（优化 tina_status_ext 更新）
- 编译优化未开启（检查 -O2 或 -O3）

---

## 📝 问题日志

> 在实施过程中记录遇到的问题和解决方案

### [日期] 问题 1：标题

**描述**: 
**影响**: 
**解决方案**: 
**状态**: [ ] 待解决 / [ ] 已解决 / [ ] 已规避

---

**文档版本**: 1.0  
**创建日期**: 2025-11-01  
**最后更新**: 2025-11-01  
**作者**: [您的名字]  
**审核者**: [待定]
