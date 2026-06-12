#pragma once

#include "Debugger.h"
#include "QuickJSDebug.h"

#include <functional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class DAPServer
{
public:
    DAPServer();

    ~DAPServer();

    // Blocking — reads DAP messages from stdin, writes to stdout.
    void Run();

private:
    JSRuntime* m_rt = nullptr;

    JSContext* m_ctx = nullptr;

    Debugger m_debugger;

    std::thread m_jsThread;

    std::string m_scriptFile;

    int m_nextSeq = 1;

    bool m_running = false;

    // DAP protocol helpers
    std::string readMessage();

    void writeMessage(const std::string& json);

    void sendResponse(int requestSeq, const std::string& command,
                      bool success, const std::string& body = "");

    void sendEvent(const std::string& event, const std::string& body = "");

    // DAP command handlers
    void handleRequest(const std::string& command, const std::string& args, int requestSeq);

    void onInitialize(int requestSeq, const std::string& args);

    void onLaunch(int requestSeq, const std::string& args);

    void onSetBreakpoints(int requestSeq, const std::string& args);

    void onConfigurationDone(int requestSeq);

    void onThreads(int requestSeq);

    void onStackTrace(int requestSeq, const std::string& args);

    void onScopes(int requestSeq, const std::string& args);

    void onVariables(int requestSeq, const std::string& args);

    void onContinue(int requestSeq);

    void onPause(int requestSeq);

    void onNext(int requestSeq);

    void onStepIn(int requestSeq);

    void onStepOut(int requestSeq);

    void onDisconnect(int requestSeq);

    // Script execution
    void startScript();

    void waitForStop();

    // Source mapping (line numbers from DAP are 1-based, but we use DAP's convention)
    std::string m_launchScript;  // source code from launch args or file
};
