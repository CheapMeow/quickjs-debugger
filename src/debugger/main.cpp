#include "Debugger.h"
#include "QuickJSDebug.h"

extern "C"
{
#include "quickjs.h"
}

#include <chrono>
#include <iostream>
#include <thread>

static JSValue Eval(JSContext* ctx, const char* code, const char* filename)
{
    return JS_Eval(ctx, code, strlen(code), filename, JS_EVAL_TYPE_GLOBAL);
}

int main()
{
    JSRuntime* rt = JS_NewRuntime();

    if (!rt)
    {
        std::cout << "Failed to create runtime\n";

        return -1;
    }

    JSContext* ctx = JS_NewContext(rt);

    if (!ctx)
    {
        std::cout << "Failed to create context\n";

        JS_FreeRuntime(rt);

        return -1;
    }

    Debugger debugger;

    InstallQuickJSDebugger(rt, ctx, &debugger);

    std::thread jsThread([&]() {
        const char* script = R"(

function foo()
{
    while (true)
    {
        let x = Math.random();
    }
}

function bar()
{
    foo();
}

bar();

)";

        JSValue result = Eval(ctx, script, "test.js");

        if (JS_IsException(result))
        {
            JSValue exception = JS_GetException(ctx);

            const char* err = JS_ToCString(ctx, exception);

            if (err)
            {
                std::cout << err << "\n";

                JS_FreeCString(ctx, err);
            }

            JS_FreeValue(ctx, exception);
        }

        JS_FreeValue(ctx, result);
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\nRequesting pause...\n";

    debugger.RequestPause();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto frames = debugger.GetStackFrames();

    std::cout << "\n=== STACK FRAMES (" << frames.size() << " total) ===\n";

    for (size_t i = 0; i < frames.size(); i++)
    {
        std::cout << "#" << i << " "
                  << frames[i].functionName << "() at "
                  << frames[i].filename << ":"
                  << frames[i].line << ":"
                  << frames[i].column << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "\nResuming VM..." << std::endl;

    debugger.Resume();

    jsThread.join();

    JS_FreeContext(ctx);

    JS_FreeRuntime(rt);

    return 0;
}
