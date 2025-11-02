# QuickJS 扩展模块

这个目录包含了为 QuickJS 开发的通用扩展，补充 QuickJS 缺失的功能。

## 扩展列表

### 1. quickjs_coroutine
**功能**：为 QuickJS 提供协程支持
- 类似 Lua 的 coroutine.yield/resume
- 管理 generator 的 session 映射
- 线程安全的协程恢复机制

**文件**：
- `quickjs_coroutine.c` - 实现
- `quickjs_coroutine.h` - 接口

**用途**：让 JavaScript generator 能像 Lua 协程一样工作

### 2. quickjs_searchpath
**功能**：提供 Lua 风格的文件查找功能
- 完全对齐 Lua 的 package.searchpath
- 支持路径模板（`?` 占位符）
- 支持多路径搜索（`;` 分隔）
- 点号自动转换为路径分隔符

**文件**：
- `quickjs_searchpath.c` - 实现（基于 Lua loadlib.c）
- `quickjs_searchpath.h` - 接口

**用途**：动态查找和加载模块文件

## 设计原则

1. **通用性** - 不依赖特定项目，任何 QuickJS 项目都可使用
2. **对齐 Lua** - 行为和接口尽量与 Lua 保持一致
3. **独立性** - 每个扩展独立，可单独使用
4. **高性能** - C 实现，最小化开销

## 使用方式

### 编译时包含
```makefile
# 在你的项目中
SOURCES += quickjs_generator/quickjs_coroutine.c
SOURCES += quickjs_generator/quickjs_searchpath.c
```

### 初始化
```c
// 在 main.c 中
#include "quickjs_generator/quickjs_coroutine.h"
#include "quickjs_generator/quickjs_searchpath.h"

int main() {
    JSContext *ctx = JS_NewContext(rt);

    // 初始化扩展
    JS_InitCoroutine(ctx);      // 协程支持
    js_init_searchpath(ctx);     // 文件查找

    // ...
}
```

### JavaScript 中使用
```javascript
// 使用 searchPath
const path = searchPath("module.name", "src/?.js;lib/?.js");

// 使用协程（通过 yield）
function* myGenerator() {
    const result = yield session_id;
    // ...
}
```

## 为什么放在这里？

1. **不是 jtask 特有** - 这些是通用功能，不仅 jtask 可用
2. **补充 QuickJS** - QuickJS 本身缺少这些功能
3. **便于复用** - 其他项目也可以使用这些扩展
4. **清晰的层次** - 语言扩展 vs 应用框架

## 未来扩展

可能添加的扩展：
- `quickjs_debug.c` - 统一的调试接口
- `quickjs_fs_extra.c` - 增强的文件系统操作
- `quickjs_process.c` - 进程管理功能

---

**核心理念**：把通用功能做成 QuickJS 扩展，而不是塞进应用代码里。