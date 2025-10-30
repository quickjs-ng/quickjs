# Stackful 协程的魔法原理

## 问题：上下文怎么保存的？

### C 栈里有什么？

```
栈帧（Stack Frame）:
┌─────────────────────┐ 高地址
│ outer() 的栈帧      │
│  - 返回地址         │
│  - 局部变量         │
│  - this 指针        │
├─────────────────────┤
│ inner() 的栈帧      │
│  - 返回地址         │
│  - 局部变量         │
│  - 参数             │
├─────────────────────┤
│ mock_call() 的栈帧  │
│  - 返回地址         │  <- 这里调用 yield
│  - ctx 参数         │
├─────────────────────┤
│ stackful_yield()    │ <- 当前栈顶
└─────────────────────┘ 低地址
```

**yield 时做了什么？**

看 [quickjs_stackful_mini.c](../3rd/quickjs_generator/quickjs_stackful_mini.c)：

```c
void stackful_yield(stackful_schedule *S) {
    int id = S->running;
    mco_coro *co = S->coroutines[id];

    // minicoro 内部做了这个：
    mco_yield(co);
    // ↓
    // 1. 保存 CPU 寄存器（RSP, RBP, RIP, ...）
    // 2. 复制整个栈内存（从栈底到当前栈顶）
    // 3. 切换回主栈
}
```

### minicoro 内部机制（简化版）

```c
// yield 时
void mco_yield(mco_coro* co) {
    // 1. 保存当前栈的内容
    void *stack_bottom = co->stack_base;
    void *stack_top = get_current_sp();  // 当前栈指针
    size_t stack_size = stack_bottom - stack_top;

    // 2. 复制到堆上
    memcpy(co->saved_stack, stack_top, stack_size);

    // 3. 保存 CPU 状态
    co->saved_rsp = stack_top;
    co->saved_rbp = get_rbp();
    co->saved_rip = get_rip();

    // 4. 切换回主栈
    switch_context(&co->context, &main_context);
}

// resume 时
void mco_resume(mco_coro* co) {
    // 1. 恢复栈内容
    void *stack_top = co->saved_rsp;
    memcpy(stack_top, co->saved_stack, co->stack_size);

    // 2. 恢复 CPU 状态
    set_rsp(co->saved_rsp);
    set_rbp(co->saved_rbp);
    set_rip(co->saved_rip);

    // 3. 切换回协程栈
    switch_context(&main_context, &co->context);
    // 现在 CPU 继续从 yield 点执行
}
```

## this 怎么办？

**this 就在栈上！**

### JavaScript 调用链

```javascript
const obj = {
    method: function() {
        console.log(this);  // this = obj
        mock_call();        // 这里会 yield
        console.log(this);  // resume 后 this 还是 obj
    }
};
obj.method();
```

### C 层实现

QuickJS 的 `this` 是通过参数传递的：

```c
static JSValue js_object_method(JSContext *ctx, JSValueConst this_val, ...) {
    // this_val 在栈上！

    // yield 前
    printf("this = %p\n", JS_VALUE_GET_PTR(this_val));

    stackful_yield(S);

    // yield 后
    printf("this = %p\n", JS_VALUE_GET_PTR(this_val));
    // ↑ 指针还是一样的！因为整个栈帧都被保存和恢复了
}
```

### 栈帧里的 this

```
js_object_method 的栈帧:
┌───────────────────┐
│ 返回地址          │
├───────────────────┤
│ ctx 参数          │
├───────────────────┤
│ this_val 参数     │ <- 指向 obj 对象
├───────────────────┤
│ argc              │
├───────────────────┤
│ argv[]            │
└───────────────────┘

yield 时：整个栈帧复制到堆上
resume 时：整个栈帧恢复回栈上
         ↓
所有局部变量、参数（包括 this_val）都恢复了！
```

## 匿名函数为什么也行？

**因为闭包也在栈上（或通过指针引用堆上的对象）！**

### 例子

```javascript
function outer() {
    const captured = 42;  // 被闭包捕获

    const anonymous = function() {
        console.log(captured);  // 访问闭包变量
        mock_call();            // yield
        console.log(captured);  // resume 后还能访问
    };

    return anonymous();
}
```

### QuickJS 内部

```c
// QuickJS 把闭包变量存在一个特殊对象里
struct JSFunctionBytecode {
    JSValue *closure_var;  // 指向闭包变量数组
};

// 调用匿名函数时
static JSValue js_anonymous_func(JSContext *ctx, JSValueConst this_val, ...) {
    JSFunctionBytecode *func = get_current_function(ctx);
    JSValue captured = func->closure_var[0];  // captured 变量

    // 这些指针都在栈上！
    printf("captured = %d\n", JS_VALUE_GET_INT(captured));

    stackful_yield(S);

    // resume 后，func 和 captured 指针都还在！
    printf("captured = %d\n", JS_VALUE_GET_INT(captured));
}
```

### 为什么能工作？

```
栈帧:
┌────────────────────┐
│ func 指针 ────────┼─> JSFunctionBytecode (在堆上)
│                    │       │
│ captured 指针 ────┼───────┘-> closure_var[0] (在堆上)
│                    │
│ ...                │
└────────────────────┘

yield 保存的是这个栈帧（包括所有指针）
resume 恢复后，指针还指向同一个堆对象
    ↓
闭包变量还在！
```

## 箭头函数？

一样的原理！

```javascript
const arrow = (x) => {
    console.log(x);    // x 在栈上
    mock_call();       // yield
    console.log(x);    // resume 后 x 还在
};
```

箭头函数只是语法糖，编译后和普通函数一样，参数 `x` 在栈帧里。

## 回调函数？

```javascript
[1, 2].forEach((item) => {
    console.log(item);  // item 在栈上
    mock_call();        // yield
    console.log(item);  // resume 后 item 还在
});
```

### forEach 的调用栈

```
forEach 循环第一次:
┌─────────────────────┐
│ forEach 内部        │
├─────────────────────┤
│ 回调函数栈帧        │
│  - item = 1         │ <- yield 在这里
└─────────────────────┘

yield 后保存了整个栈，包括 forEach 的状态和 item=1

resume 后恢复，继续执行
```

## 总结

**Stackful 协程的核心**：

1. **保存整个 C 栈**（包括所有栈帧、局部变量、参数、返回地址）
2. **保存 CPU 寄存器**（RSP, RBP, RIP 等）
3. **Resume 时完整恢复**

**为什么什么都能保存？**

- ✅ **局部变量** - 在栈上，整个复制
- ✅ **函数参数** - 在栈上，整个复制
- ✅ **this 指针** - 在栈上（作为参数），整个复制
- ✅ **返回地址** - 在栈上，整个复制
- ✅ **闭包变量** - 指针在栈上，指向堆对象（堆对象不会消失）
- ✅ **寄存器** - 显式保存和恢复

**唯一不保存的**：

- ❌ **全局变量** - 不在栈上，但也不需要保存（全局的）
- ❌ **堆对象** - 不在栈上，由 GC 管理

## 对比 Generator

**Generator (stackless)**：
```javascript
function* gen() {
    const x = 1;
    yield x;  // 只能保存 gen 函数自己的局部变量
}
```

**Stackful**：
```javascript
function f1() {
    function f2() {
        function f3() {
            mock_call();  // 保存 f1/f2/f3 所有栈帧！
        }
        f3();
    }
    f2();
}
```

这就是 **stackful** 的魔法！
