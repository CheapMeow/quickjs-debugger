#include "QuickJSDebug.h"

#include "Debugger.h"

extern "C"
{
#include "quickjs.h"
#include "quickjs-debugger.h"
}

#include <iostream>

static JSContext* g_ctx = nullptr;

static int interrupt_handler(JSRuntime* rt, void* opaque)
{
    auto* debugger = static_cast<Debugger*>(opaque);

    if (!debugger->ShouldPause())
    {
        return 0;
    }

    std::cout << "\nInterrupt handler triggered\n";

    JSDebugLocation raw = {};

    JSLocation location;

    if (JS_GetCurrentLocation(g_ctx, &raw))
    {
        const char* filename = JS_AtomToCString(g_ctx, raw.filename);

        location.filename = filename ? filename : "<unknown>";
        location.line     = raw.line;
        location.column   = raw.col;

        JS_FreeCString(g_ctx, filename);
    }
    else
    {
        location.filename = "<unknown>";
        location.line     = -1;
        location.column   = -1;
    }

    PrintJSStackTrace(g_ctx);

    debugger->SuspendVM(location);

    return 0;
}

void InstallQuickJSDebugger(JSRuntime* rt, JSContext* ctx, Debugger* debugger)
{
    g_ctx = ctx;

    JS_SetInterruptHandler(rt, interrupt_handler, debugger);
}

void PrintJSStackTrace(JSContext* ctx)
{
    const char* code = R"(

(function()
{
    const e = new Error();

    return e.stack;

})()

)";

    JSValue result = JS_Eval(ctx, code, strlen(code), "<stacktrace>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result))
    {
        std::cout << "Failed to get stacktrace\n";

        return;
    }

    const char* str = JS_ToCString(ctx, result);

    if (str)
    {
        std::cout << "\n=== JS STACKTRACE ===\n";
        std::cout << str << "\n";

        JS_FreeCString(ctx, str);
    }

    JS_FreeValue(ctx, result);
}