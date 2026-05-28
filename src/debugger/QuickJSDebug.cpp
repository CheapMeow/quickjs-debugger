#include "QuickJSDebug.h"

#include "Debugger.h"

extern "C"
{
#include "quickjs.h"
#include "quickjs-debugger.h"
}

static JSContext* g_ctx = nullptr;

static int interrupt_handler(JSRuntime* rt, void* opaque)
{
    auto* debugger = static_cast<Debugger*>(opaque);

    if (!debugger->ShouldPause())
    {
        return 0;
    }

    JSDebugFrame raw[256];
    int count = JS_GetStackFrames(g_ctx, raw, 256);

    std::vector<StackFrame> frames;
    frames.reserve(count);

    for (int i = 0; i < count; i++)
    {
        StackFrame frame;

        const char* s = JS_AtomToCString(g_ctx, raw[i].filename);
        frame.filename = s ? s : "<unknown>";
        JS_FreeCString(g_ctx, s);

        s = JS_AtomToCString(g_ctx, raw[i].func_name);
        frame.functionName = s ? s : "<anonymous>";
        JS_FreeCString(g_ctx, s);

        frame.line   = raw[i].line;
        frame.column = raw[i].col;
        frame.pc     = raw[i].pc;

        frames.push_back(std::move(frame));
    }

    debugger->SuspendVM(frames);

    return 0;
}

void InstallQuickJSDebugger(JSRuntime* rt, JSContext* ctx, Debugger* debugger)
{
    g_ctx = ctx;

    JS_SetInterruptHandler(rt, interrupt_handler, debugger);
}
