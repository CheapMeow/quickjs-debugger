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

static void dump_error(JSContext* ctx)
{
    JSValue exception = JS_GetException(ctx);

    const char* err = JS_ToCString(ctx, exception);

    if (err)
    {
        std::cout << "Uncaught: " << err << std::endl;

        JS_FreeCString(ctx, err);
    }

    JS_FreeValue(ctx, exception);
}

static void print_variables(JSContext* ctx, int frameCount)
{
    for (int fi = 0; fi < frameCount && fi < 3; fi++)
    {
        auto vars = GetFrameVariables(ctx, fi);

        if (vars.empty())
            continue;

        std::cout << "  Frame #" << fi << " variables (" << vars.size() << "):" << std::endl;

        for (const auto& v : vars)
        {
            std::cout << "    " << v.name << " = " << v.value << std::endl;
        }
    }
}

int main()
{
    JSRuntime* rt = JS_NewRuntime();

    if (!rt)
    {
        std::cout << "Failed to create runtime" << std::endl;

        return -1;
    }

    JSContext* ctx = JS_NewContext(rt);

    if (!ctx)
    {
        std::cout << "Failed to create context" << std::endl;

        JS_FreeRuntime(rt);

        return -1;
    }

    Debugger debugger;

    InstallQuickJSDebugger(rt, ctx, &debugger);

    // ---- Phase 1.3: breakpoint demo ----

    std::cout << "\n========== PHASE 1.3: BREAKPOINT ==========" << std::endl;

    debugger.AddBreakpoint("loop.js", 6);

    std::thread t1([&]() {
        const char* script = R"(
function run()
{
    let n = 0;
    while (n < 5000000)
        n = n + 1;
}
run();
)";
        JSValue result = Eval(ctx, script, "loop.js");

        if (JS_IsException(result))
            dump_error(ctx);

        JS_FreeValue(ctx, result);
    });

    std::cout << "Waiting for breakpoint..." << std::endl;

    debugger.WaitUntilPaused();

    {
        auto frames = debugger.GetStackFrames();

        std::cout << "\n=== BREAKPOINT HIT ===" << std::endl;
        std::cout << "Stack frames (" << frames.size() << " total):" << std::endl;

        for (size_t i = 0; i < frames.size(); i++)
        {
            std::cout << "  #" << i << " "
                      << frames[i].functionName << "() at "
                      << frames[i].filename << ":"
                      << frames[i].line << ":"
                      << frames[i].column << std::endl;
        }

        print_variables(ctx, (int)frames.size());
    }

    debugger.ClearBreakpoints();

    std::cout << "\nResuming VM..." << std::endl;

    debugger.Resume();

    t1.join();

    // ---- manual pause demo ----

    std::cout << "\n========== MANUAL PAUSE ==========" << std::endl;

    std::thread t2([&]() {
        const char* script = R"(
function run()
{
    let n = 0;
    while (n < 5000000)
        n = n + 1;
}
run();
)";
        JSValue result = Eval(ctx, script, "loop2.js");

        if (JS_IsException(result))
            dump_error(ctx);

        JS_FreeValue(ctx, result);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    debugger.RequestPause();

    debugger.WaitUntilPaused();

    {
        auto frames = debugger.GetStackFrames();

        std::cout << "\n=== MANUAL PAUSE ===" << std::endl;
        std::cout << "Stack frames (" << frames.size() << " total):" << std::endl;

        for (size_t i = 0; i < frames.size(); i++)
        {
            std::cout << "  #" << i << " "
                      << frames[i].functionName << "() at "
                      << frames[i].filename << ":"
                      << frames[i].line << ":"
                      << frames[i].column << std::endl;
        }

        print_variables(ctx, (int)frames.size());
    }

    debugger.Resume();

    t2.join();

    // ---- Phase 1.4: exception pause demo ----

    std::cout << "\n========== PHASE 1.4: EXCEPTION PAUSE ==========" << std::endl;

    debugger.SetExceptionPauseEnabled(true);

    std::thread t3([&]() {
        const char* script = R"(
function baz()
{
    let msg = "hello";
    let count = 42;
    let flag = true;
    let obj = {};
    let nothing = null;
    throw new Error("test exception from baz");
}
function bar()
{
    baz();
}
bar();
)";
        JSValue result = Eval(ctx, script, "exception_test.js");

        if (JS_IsException(result))
            dump_error(ctx);

        JS_FreeValue(ctx, result);
    });

    std::cout << "Waiting for exception..." << std::endl;

    debugger.WaitUntilPaused();

    {
        auto frames = debugger.GetStackFrames();

        std::cout << "\n=== EXCEPTION PAUSED ===" << std::endl;
        std::cout << "Stack frames (" << frames.size() << " total):" << std::endl;

        for (size_t i = 0; i < frames.size(); i++)
        {
            std::cout << "  #" << i << " "
                      << frames[i].functionName << "() at "
                      << frames[i].filename << ":"
                      << frames[i].line << ":"
                      << frames[i].column << std::endl;
        }

        print_variables(ctx, (int)frames.size());
    }

    std::cout << "\nResuming (exception will propagate)..." << std::endl;

    debugger.Resume();

    t3.join();

    // ---- Phase 1.5: variables demo ----

    std::cout << "\n========== PHASE 1.5: VARIABLE INSPECTION ==========" << std::endl;

    std::thread t4([&]() {
        const char* script = R"(
function compute(a, b)
{
    let sum = a + b;
    let product = a * b;
    let msg = "done";
    let flag = true;
    let obj = { x: 10, y: 20 };
    let arr = [1, 2, 3];
    let n = 0;
    while (n < 1000000)
        n = n + 1;
    return sum;
}
compute(3, 4);
)";
        JSValue result = Eval(ctx, script, "vars.js");

        if (JS_IsException(result))
            dump_error(ctx);

        JS_FreeValue(ctx, result);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    debugger.RequestPause();

    std::cout << "Waiting for pause..." << std::endl;

    debugger.WaitUntilPaused();

    {
        auto frames = debugger.GetStackFrames();

        std::cout << "\n=== BREAKPOINT HIT (variable demo) ===" << std::endl;
        std::cout << "Stack frames (" << frames.size() << " total):" << std::endl;

        for (size_t i = 0; i < frames.size(); i++)
        {
            std::cout << "  #" << i << " "
                      << frames[i].functionName << "() at "
                      << frames[i].filename << ":"
                      << frames[i].line << ":"
                      << frames[i].column << std::endl;
        }

        print_variables(ctx, (int)frames.size());
    }

    debugger.Resume();

    t4.join();

    // ---- cleanup ----

    std::cout << "\n========== ALL DONE ==========" << std::endl;

    JS_FreeContext(ctx);

    JS_FreeRuntime(rt);

    return 0;
}
