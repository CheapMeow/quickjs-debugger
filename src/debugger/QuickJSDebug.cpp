#include "QuickJSDebug.h"

#include "Debugger.h"

extern "C"
{
#include "quickjs.h"
#include "quickjs-debugger.h"
}

static JSContext* g_ctx = nullptr;

static void collect_and_suspend(Debugger* debugger)
{
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
}

static int interrupt_handler(JSRuntime* rt, void* opaque)
{
    auto* debugger = static_cast<Debugger*>(opaque);

    bool shouldPause = debugger->ShouldPause();

    if (!shouldPause)
    {
        JSDebugLocation rawLoc = {};

        if (JS_GetCurrentLocation(g_ctx, &rawLoc))
        {
            const char* filename = JS_AtomToCString(g_ctx, rawLoc.filename);

            if (filename)
            {
                shouldPause = debugger->ShouldBreak(filename, rawLoc.line);

                JS_FreeCString(g_ctx, filename);
            }
        }
    }

    if (!shouldPause)
        return 0;

    collect_and_suspend(debugger);

    return 0;
}

static void exception_handler(JSContext* ctx, void* opaque)
{
    auto* debugger = static_cast<Debugger*>(opaque);

    if (!debugger->IsExceptionPauseEnabled())
        return;

    collect_and_suspend(debugger);
}

void InstallQuickJSDebugger(JSRuntime* rt, JSContext* ctx, Debugger* debugger)
{
    g_ctx = ctx;

    JS_SetInterruptHandler(rt, interrupt_handler, debugger);

    JS_SetExceptionHandler(rt, exception_handler, debugger);
}
