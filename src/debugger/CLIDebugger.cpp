#include "CLIDebugger.h"

extern "C"
{
#include "quickjs.h"
#include "quickjs-debugger.h"
}

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <streambuf>

CLIDebugger::CLIDebugger()
{
    m_rt = JS_NewRuntime();

    if (!m_rt)
    {
        std::cerr << "Failed to create JSRuntime" << std::endl;

        return;
    }

    m_ctx = JS_NewContext(m_rt);

    if (!m_ctx)
    {
        std::cerr << "Failed to create JSContext" << std::endl;

        JS_FreeRuntime(m_rt);

        m_rt = nullptr;

        return;
    }

    InstallQuickJSDebugger(m_rt, m_ctx, &m_debugger);
}

CLIDebugger::~CLIDebugger()
{
    m_running = false;

    if (m_jsThread.joinable())
        m_jsThread.join();

    if (m_ctx)
        JS_FreeContext(m_ctx);

    if (m_rt)
        JS_FreeRuntime(m_rt);
}

bool CLIDebugger::LoadScript(const std::string& filePath)
{
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        std::cerr << "Cannot open file: " << filePath << std::endl;

        return false;
    }

    m_scriptSource.assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());

    m_scriptFile = filePath;

    std::cout << "Loaded " << filePath << " (" << m_scriptSource.size() << " bytes)" << std::endl;

    return true;
}

void CLIDebugger::startScript()
{
    m_debugger.SetExceptionPauseEnabled(true);

    m_jsThread = std::thread([this]() {
        JSValue result = JS_Eval(
            m_ctx, m_scriptSource.c_str(), m_scriptSource.size(),
            m_scriptFile.c_str(), JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(result))
        {
            JSValue exception = JS_GetException(m_ctx);

            const char* err = JS_ToCString(m_ctx, exception);

            if (err)
            {
                std::cerr << "Uncaught: " << err << std::endl;

                JS_FreeCString(m_ctx, err);
            }

            JS_FreeValue(m_ctx, exception);
        }

        JS_FreeValue(m_ctx, result);

        m_debugger.SignalFinished();
    });
}

void CLIDebugger::Run()
{
    if (!m_rt || !m_ctx)
    {
        std::cerr << "Debugger not initialized" << std::endl;

        return;
    }

    std::cout << "\nQuickJS CLI Debugger" << std::endl;
    std::cout << "Type 'help' for commands, 'quit' to exit." << std::endl;

    m_running = true;

    commandLoop();
}

void CLIDebugger::commandLoop()
{
    while (m_running)
    {
        std::cout << "(qjsdb) " << std::flush;

        std::string line;

        if (!std::getline(std::cin, line))
        {
            m_running = false;

            break;
        }

        if (line.empty())
            continue;

        executeCommand(line);
    }
}

void CLIDebugger::executeCommand(const std::string& line)
{
    std::istringstream iss(line);

    std::string cmd;

    iss >> cmd;

    if (cmd == "quit" || cmd == "q")
    {
        m_running = false;
    }
    else if (cmd == "run" || cmd == "r")
    {
        cmdRun();
    }
    else if (cmd == "break" || cmd == "b")
    {
        std::string arg;

        iss >> arg;

        auto colon = arg.find(':');

        if (colon != std::string::npos)
        {
            std::string file = arg.substr(0, colon);
            int lineNo = std::stoi(arg.substr(colon + 1));

            cmdBreak(file, lineNo);
        }
        else
        {
            std::cerr << "Usage: break <file>:<line>" << std::endl;
        }
    }
    else if (cmd == "continue" || cmd == "c")
    {
        cmdContinue();
    }
    else if (cmd == "step" || cmd == "s")
    {
        cmdStep();
    }
    else if (cmd == "next" || cmd == "n")
    {
        cmdNext();
    }
    else if (cmd == "finish")
    {
        cmdFinish();
    }
    else if (cmd == "backtrace" || cmd == "bt")
    {
        cmdBacktrace();
    }
    else if (cmd == "locals")
    {
        cmdLocals();
    }
    else if (cmd == "pause")
    {
        cmdPause();
    }
    else if (cmd == "help" || cmd == "h")
    {
        cmdHelp();
    }
    else
    {
        std::cerr << "Unknown command: " << cmd << std::endl;
    }
}

void CLIDebugger::displayPauseLocation()
{
    auto frames = m_debugger.GetStackFrames();

    if (frames.empty())
        return;

    const auto& top = frames[0];

    std::cout << "\nPaused at " << top.filename << ":"
              << top.line << ":" << top.column
              << "  [" << top.functionName << "]"
              << "  (depth " << frames.size() << ")"
              << std::endl;
}

void CLIDebugger::cmdRun()
{
    if (m_scriptSource.empty())
    {
        std::cerr << "No script loaded. Use: Debugger <script.js>" << std::endl;

        return;
    }

    if (m_jsThread.joinable())
    {
        std::cerr << "Script already running" << std::endl;

        return;
    }

    std::cout << "Running " << m_scriptFile << "..." << std::endl;

    startScript();

    resumeAndWait();
}

void CLIDebugger::cmdBreak(const std::string& file, int line)
{
    m_debugger.AddBreakpoint(file, line);
}

void CLIDebugger::cmdContinue()
{
    if (!m_debugger.IsPaused())
    {
        std::cerr << "Not paused" << std::endl;

        return;
    }

    resumeAndWait();
}

void CLIDebugger::cmdStep()
{
    if (!m_debugger.IsPaused())
    {
        std::cerr << "Not paused" << std::endl;

        return;
    }

    auto frames = m_debugger.GetStackFrames();

    if (frames.empty())
        return;

    m_debugger.StepInto(frames[0].filename, frames[0].line, frames[0].pc, (int)frames.size());

    resumeAndWait();
}

void CLIDebugger::cmdNext()
{
    if (!m_debugger.IsPaused())
    {
        std::cerr << "Not paused" << std::endl;

        return;
    }

    auto frames = m_debugger.GetStackFrames();

    if (frames.empty())
        return;

    m_debugger.StepOver(frames[0].filename, frames[0].line, frames[0].pc, (int)frames.size());

    resumeAndWait();
}

void CLIDebugger::cmdFinish()
{
    if (!m_debugger.IsPaused())
    {
        std::cerr << "Not paused" << std::endl;

        return;
    }

    auto frames = m_debugger.GetStackFrames();

    if (frames.empty())
        return;

    m_debugger.ClearBreakpoints();

    m_debugger.StepOut((int)frames.size());

    resumeAndWait();
}

void CLIDebugger::cmdBacktrace()
{
    if (!m_debugger.IsPaused())
    {
        std::cerr << "Not paused" << std::endl;

        return;
    }

    auto frames = m_debugger.GetStackFrames();

    std::cout << "\nStack frames (" << frames.size() << " total):\n";

    for (size_t i = 0; i < frames.size(); i++)
    {
        std::cout << "  #" << i << " "
                  << frames[i].functionName << "() at "
                  << frames[i].filename << ":"
                  << frames[i].line << ":"
                  << frames[i].column << "\n";
    }
}

void CLIDebugger::cmdLocals()
{
    if (!m_debugger.IsPaused())
    {
        std::cerr << "Not paused" << std::endl;

        return;
    }

    auto frames = m_debugger.GetStackFrames();

    for (int fi = 0; fi < (int)frames.size() && fi < 3; fi++)
    {
        auto vars = GetFrameVariables(m_ctx, fi);

        if (vars.empty())
            continue;

        std::cout << "Frame #" << fi << " ("
                  << frames[fi].functionName << ") variables:\n";

        for (const auto& v : vars)
        {
            std::cout << "  " << v.name << " = " << v.value << "\n";
        }
    }
}

void CLIDebugger::cmdPause()
{
    if (m_debugger.IsPaused())
    {
        std::cerr << "Already paused" << std::endl;

        return;
    }

    std::cout << "Requesting pause..." << std::endl;

    m_debugger.RequestPause();

    m_debugger.WaitUntilPaused();
}

void CLIDebugger::cmdHelp()
{
    std::cout << "\nCommands:\n"
              << "  run / r                  Start script execution\n"
              << "  break / b <file>:<line>  Set breakpoint\n"
              << "  continue / c             Resume execution\n"
              << "  step / s                 Step into\n"
              << "  next / n                 Step over\n"
              << "  finish                   Step out\n"
              << "  backtrace / bt           Show stack frames\n"
              << "  locals                   Show local variables\n"
              << "  pause                    Interrupt running script\n"
              << "  help / h                 Show this help\n"
              << "  quit / q                 Exit debugger\n"
              << std::endl;
}

void CLIDebugger::resumeAndWait()
{
    m_debugger.Resume();

    m_debugger.WaitForPauseOrFinish();

    if (m_debugger.IsFinished())
    {
        m_jsThread.join();

        std::cout << "\nScript finished." << std::endl;

        m_running = false;
    }
    else
    {
        displayPauseLocation();
    }
}
