quickjs 的虚拟机中，代码被翻译为字节码。

执行一段字节码之后，会触发中断检查。此时可以通过 quickjs 公开的接口 `JS_SetInterruptHandler` 注入回调。

`JS_SetInterruptHandler` 可以拿到 `JSRuntime` 对象。在这个类型中，与当前执行行数相关的是 `JSStackFrame.cur_pc`，也就是只有程序计数器 PC。

```c
struct JSRuntime {
    // ...

    struct JSStackFrame *current_stack_frame;
};
```

```c
typedef struct JSStackFrame {
    struct JSStackFrame *prev_frame; /* NULL if first stack frame */
    JSValue cur_func; /* current function, JS_UNDEFINED if the frame is detached */
    uint8_t *cur_pc; /* only used in bytecode functions : PC of the
                        instruction after the call */
    // ...
} JSStackFrame;
```

quickjs 不保存每条指令对应的行号。所以我们需要从 `PC` 映射到 `line`，也就是行号和列号。

quickjs.c 已经提供了从 `PC` 映射到 `line` 的函数，但是它未公开：

```c
static int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                         uint32_t pc_value, int *pcol_num)
```

这里可以看到它是怎么解压 `b` 指向的字节流，得到行号和列号的。

理解了之后，我们就知道怎么使用这个内部函数。在 quickjs.c 新写一个函数，并新写一个头文件，声明，公开它：

```c
// [Debugger Begin] PC to line
int JS_GetCurrentLocation(
    JSContext* ctx,
    JSDebugLocation* out_loc
)
{
    JSStackFrame* sf;
    JSFunctionBytecode* b;
    uint32_t pc;

    sf = ctx->rt->current_stack_frame;

    if (!sf)
        return 0;

    if (!js_is_bytecode_function(sf->cur_func))
        return 0;

    b = JS_VALUE_GET_PTR(sf->cur_func);

    if (!b)
        return 0;

    pc = sf->cur_pc - b->byte_code_buf;

    out_loc->filename = b->debug.filename;
    out_loc->line =
        find_line_num(ctx, b, pc);

    return 1;
}
// [Debugger End]
```