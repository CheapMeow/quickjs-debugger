# QuickJS + Rider 调试器 Roadmap

目标：

我有一个嵌入 quickjs-ng 的 C++ 项目。

我希望：

* Rider 可以同时调试 C++ 和 JS
* JS 文件可以下断点
* JS 命中断点时 IDE 自动暂停
* Rider 能显示 JS stack / variables
* JS exception 能自动中断
* 支持 step in / over / out
* 最终通过 DAP 接入 Rider

整个系统最终结构：

```text
Rider
  ↓
DAP
  ↓
QuickJS Debug Adapter
  ↓
QuickJS Debug Core
  ↓
quickjs-ng VM
```

真正困难的部分其实不是 DAP，而是：

```text
QuickJS Debug Core
```

也就是：

* 如何暂停 VM
* 如何拿 stack
* 如何获取变量
* 如何单步
* 如何断点

DAP 只是把这些功能暴露给 IDE。

---

# 第一阶段：VM Debug Core

目标：

让 QuickJS 真正具备“调试器能力”。

这是整个项目最核心的部分。

---

## 1. 当前执行位置（进行中）

已经完成：

* JS_SetInterruptHandler
* Pause / Resume
* JS_GetCurrentLocation

当前需要：

* 验证 line/column 是否稳定
* 验证函数调用场景
* 验证 exception 场景
* 验证 native call 场景

建议：

JSDebugLocation 增加：

```cpp
uint32_t pc;
```

后面 stepping 必须使用。

---

## 2. Stack Frame 遍历

不要再使用：

```js
new Error().stack
```

真正 debugger 应该直接遍历：

```cpp
JSStackFrame
```

目标：

实现：

```cpp
JS_GetStackFrames(...)
```

返回：

```text
foo (a.js:10)
bar (b.js:20)
main (main.js:1)
```

同时后面 variables 也依赖 stack frame。

这是下一阶段最重要的事情。

---

## 3. Breakpoint System

需要实现：

```cpp
Breakpoint {
    file
    line
}
```

然后：

interrupt handler 内：

```cpp
loc = GetCurrentLocation()

if breakpoint matched:
    pause
```

注意：

不要在 interrupt handler 内：

* 分配内存
* eval JS
* 创建异常
* 调复杂 API

interrupt handler 只做：

```text
读取状态
决定是否暂停
```

---

## 4. Exception Pause

修改 QuickJS：

在：

```cpp
JS_Throw
```

或者：

```cpp
throw_exception
```

路径中：

通知 debugger：

```text
exception thrown
```

然后：

```cpp
debugger->PauseOnException(...)
```

目标：

Rider 能实现：

```text
Pause on Exceptions
```

---

## 5. Variables / Scope

需要实现：

```cpp
JS_GetScopes(frame)
JS_GetVariables(scope)
```

至少支持：

* locals
* closure
* arguments
* this

这是 DAP variables 的基础。

---

## 6. Step 系统

实现：

* step into
* step over
* step out

需要：

* pc tracking
* frame depth tracking
* previous location

这是 debugger 真正开始复杂的阶段。

建议：

breakpoint 系统稳定后再做。

---

# 第二阶段：CLI Debugger

目标：

先不要 IDE。

先做一个：

```text
gdb 风格
```

的命令行调试器。

例如：

```text
break foo.js:10
continue
step
next
bt
locals
```

原因：

DAP 只是协议。

真正复杂的是：

```text
debugger runtime
```

CLI debugger 能验证：

* breakpoints
* stepping
* variables
* stack

是否稳定。

如果 CLI debugger 都不稳定：

DAP 一定更痛苦。

---

# 第三阶段：DAP Adapter

目标：

让 Rider 能连接。

你需要实现一个：

```text
DAP Server
```

通常：

```text
TCP localhost
```

或者：

```text
stdio
```

推荐：

```text
stdio
```

更简单。

---

## 最小 DAP 功能集

先只支持：

```text
initialize
launch
setBreakpoints
threads
stackTrace
scopes
variables
continue
pause
next
stepIn
stepOut
disconnect
```

够 Rider 正常调试了。

---

## DAP 架构

```text
Rider
  ↓ JSON
DAP Server
  ↓ C++
QuickJSDebugger
  ↓
QuickJS VM
```

DAP 只是：

```text
JSON-RPC 外壳
```

不要一开始就研究协议细节。

真正重要的是：

```text
你的 debugger runtime
```

---

# 第四阶段：Rider Integration

目标：

真正从 Rider 调试 JS。

方式：

Rider 启动：

```text
你的 DAP Adapter
```

然后：

* Rider 下断点
* DAP 转发给 QuickJS
* QuickJS pause
* DAP 返回 stack / variables
* Rider UI 展示

到这里：

你已经拥有：

```text
QuickJS Inspector
```

了。

---

# 第五阶段：高级功能（后期）

这些都不是第一版必须的。

---

## Mixed Stack（非常难）

目标：

```text
C++ stack
↓
JS stack
↓
native binding
↓
JS callback
```

真正混合显示。

难度很高。

因为：

Rider 的 C++ debugger 和 DAP debugger 是两套系统。

---

## Source Map

TS -> JS。

---

## Async Stack

Promise / await。

---

## Live Edit

运行时替换函数。

---

## Heap / Profiler

后期。

---

# 推荐开发顺序

正确顺序：

```text
current location
    ↓
stack frames
    ↓
breakpoints
    ↓
variables
    ↓
exceptions
    ↓
stepping
    ↓
CLI debugger
    ↓
DAP adapter
    ↓
Rider integration
```

不要过早进入：

* DAP
* Rider
* UI

否则会在 debugger core 不稳定时进入协议地狱。

真正的核心始终是：

```text
QuickJS Debug Core
```

不是 IDE。
