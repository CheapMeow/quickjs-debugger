#pragma once

#include "Debugger.h"
#include "QuickJSDebug.h"

#include <string>
#include <thread>

class CLIDebugger
{
public:
    CLIDebugger();

    ~CLIDebugger();

    // Load a JS script file (does not run it yet).
    bool LoadScript(const std::string& filePath);

    // Enter the interactive command loop.
    void Run();

private:
    JSRuntime* m_rt = nullptr;

    JSContext* m_ctx = nullptr;

    Debugger m_debugger;

    std::thread m_jsThread;

    std::string m_scriptSource;

    std::string m_scriptFile;

    bool m_running = false;

    void startScript();

    void commandLoop();

    void executeCommand(const std::string& line);

    void displayPauseLocation();

    void cmdRun();

    void cmdBreak(const std::string& file, int line);

    void cmdContinue();

    void cmdStep();

    void cmdNext();

    void cmdFinish();

    void cmdBacktrace();

    void cmdLocals();

    void cmdPause();

    void cmdHelp();

    void resumeAndWait();
};
