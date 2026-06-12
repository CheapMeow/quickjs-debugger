#include "DAPServer.h"

extern "C"
{
#include "quickjs.h"
}

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

// ---------- JSON helpers ----------

static std::string jsonEscape(const std::string& s)
{
    std::string r;
    r.reserve(s.size() + 8);
    for (char c : s)
    {
        switch (c)
        {
        case '"':  r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n";  break;
        case '\r': r += "\\r";  break;
        case '\t': r += "\\t";  break;
        default:   r += c;      break;
        }
    }
    return r;
}

static std::string jsonGetString(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    // skip whitespace and ':'
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == ':'))
        pos++;
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++; // skip opening "
    std::string val;
    while (pos < json.size())
    {
        if (json[pos] == '\\') { pos += 2; continue; } // rough unescape
        if (json[pos] == '"') break;
        val += json[pos];
        pos++;
    }
    return val;
}

static int jsonGetInt(const std::string& json, const std::string& key, int def)
{
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == ':'))
        pos++;
    if (pos >= json.size()) return def;
    std::string num;
    if (json[pos] == '-') { num += '-'; pos++; }
    while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9')
    {
        num += json[pos];
        pos++;
    }
    if (num.empty()) return def;
    return std::stoi(num);
}

static bool jsonHasKey(const std::string& json, const std::string& key)
{
    return json.find("\"" + key + "\"") != std::string::npos;
}

static std::string jsonGetObject(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == ':'))
        pos++;
    if (pos >= json.size() || json[pos] != '{') return "";
    int depth = 1;
    size_t start = pos;
    pos++;
    while (pos < json.size() && depth > 0)
    {
        if (json[pos] == '{') depth++;
        else if (json[pos] == '}') depth--;
        pos++;
    }
    return json.substr(start, pos - start);
}

// ---------- DAPServer ----------

DAPServer::DAPServer()
{
    m_rt = JS_NewRuntime();
    if (!m_rt) return;
    m_ctx = JS_NewContext(m_rt);
    if (!m_ctx)
    {
        JS_FreeRuntime(m_rt);
        m_rt = nullptr;
        return;
    }
    InstallQuickJSDebugger(m_rt, m_ctx, &m_debugger);
    m_debugger.SetExceptionPauseEnabled(true);
}

DAPServer::~DAPServer()
{
    m_running = false;
    if (m_jsThread.joinable())
        m_jsThread.join();
    if (m_ctx)
        JS_FreeContext(m_ctx);
    if (m_rt)
        JS_FreeRuntime(m_rt);
}

// ---------- Protocol framing ----------

std::string DAPServer::readMessage()
{
    std::string line;
    int contentLength = 0;

    while (true)
    {
        int c = std::cin.get();
        if (c == EOF) return "";
        if (c == '\r') continue;
        line += (char)c;
        if (c == '\n')
        {
            if (line == "\n") break; // empty line = end of headers
            if (line.find("Content-Length:") == 0)
                contentLength = std::stoi(line.substr(15));
            line.clear();
        }
    }

    if (contentLength <= 0) return "";

    std::string body(contentLength, '\0');
    std::cin.read(&body[0], contentLength);

    return body;
}

void DAPServer::writeMessage(const std::string& json)
{
    std::cout << "Content-Length: " << json.size() << "\r\n\r\n" << json << std::flush;
}

void DAPServer::sendResponse(int requestSeq, const std::string& command,
                              bool success, const std::string& body)
{
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"response\","
        << "\"seq\":" << (m_nextSeq++) << ","
        << "\"request_seq\":" << requestSeq << ","
        << "\"command\":\"" << command << "\","
        << "\"success\":" << (success ? "true" : "false");
    if (!body.empty())
    {
        // body is a JSON object string without the outer braces, e.g.:
        // we accept both "{...}" or just "..."
        std::string b = body;
        if (b.size() > 0 && b[0] == '{') b = b.substr(1);
        if (b.size() > 0 && b.back() == '}') b.pop_back();
        if (!b.empty())
            oss << ",\"body\":{" << b << "}";
    }
    oss << "}";
    writeMessage(oss.str());
}

void DAPServer::sendEvent(const std::string& event, const std::string& body)
{
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"event\","
        << "\"seq\":" << (m_nextSeq++) << ","
        << "\"event\":\"" << event << "\"";
    if (!body.empty())
    {
        std::string b = body;
        if (b.size() > 0 && b[0] == '{') b = b.substr(1);
        if (b.size() > 0 && b.back() == '}') b.pop_back();
        if (!b.empty())
            oss << ",\"body\":{" << b << "}";
    }
    oss << "}";
    writeMessage(oss.str());
}

// ---------- Request dispatcher ----------

void DAPServer::handleRequest(const std::string& command, const std::string& args, int requestSeq)
{
    if (command == "initialize")
        onInitialize(requestSeq, args);
    else if (command == "launch")
        onLaunch(requestSeq, args);
    else if (command == "setBreakpoints")
        onSetBreakpoints(requestSeq, args);
    else if (command == "configurationDone")
        onConfigurationDone(requestSeq);
    else if (command == "threads")
        onThreads(requestSeq);
    else if (command == "stackTrace")
        onStackTrace(requestSeq, args);
    else if (command == "scopes")
        onScopes(requestSeq, args);
    else if (command == "variables")
        onVariables(requestSeq, args);
    else if (command == "continue")
        onContinue(requestSeq);
    else if (command == "pause")
        onPause(requestSeq);
    else if (command == "next")
        onNext(requestSeq);
    else if (command == "stepIn")
        onStepIn(requestSeq);
    else if (command == "stepOut")
        onStepOut(requestSeq);
    else if (command == "disconnect")
        onDisconnect(requestSeq);
    else
        sendResponse(requestSeq, command, false, "\"message\":\"unsupported command\"");
}

// ---------- Command handlers ----------

void DAPServer::onInitialize(int requestSeq, const std::string& /*args*/)
{
    sendResponse(requestSeq, "initialize", true,
        "\"supportsConfigurationDoneRequest\":true,"
        "\"supportsFunctionBreakpoints\":false,"
        "\"supportsConditionalBreakpoints\":false,"
        "\"supportsHitConditionalBreakpoints\":false,"
        "\"supportsEvaluateForHovers\":false,"
        "\"supportsStepBack\":false,"
        "\"supportsSetVariable\":false,"
        "\"supportsRestartFrame\":false,"
        "\"supportsGotoTargetsRequest\":false,"
        "\"supportsStepInTargetsRequest\":false,"
        "\"supportsCompletionsRequest\":false,"
        "\"supportsModulesRequest\":false,"
        "\"supportsExceptionOptions\":false,"
        "\"supportsValueFormattingOptions\":false,"
        "\"supportsExceptionInfoRequest\":false,"
        "\"supportTerminateDebuggee\":false,"
        "\"supportsDelayedStackTraceLoading\":false,"
        "\"supportsLoadedSourcesRequest\":false,"
        "\"supportsLogPoints\":false,"
        "\"supportsTerminateThreadsRequest\":false,"
        "\"supportsSetExpression\":false,"
        "\"supportsTerminateRequest\":false,"
        "\"supportsDataBreakpoints\":false");
}

void DAPServer::onLaunch(int requestSeq, const std::string& args)
{
    std::string program = jsonGetString(args, "program");
    if (program.empty())
        program = m_scriptFile;
    if (program.empty())
    {
        sendResponse(requestSeq, "launch", false,
            "\"message\":\"no program specified\"");
        return;
    }

    std::ifstream file(program);
    if (!file.is_open())
    {
        sendResponse(requestSeq, "launch", false,
            "\"message\":\"cannot open file\"");
        return;
    }
    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    m_scriptFile = program;
    m_launchScript = source;

    sendResponse(requestSeq, "launch", true);

    startScript();

    sendEvent("initialized");
}

void DAPServer::onSetBreakpoints(int requestSeq, const std::string& args)
{
    std::string sourceObj = jsonGetObject(args, "source");
    std::string path = jsonGetString(sourceObj, "path");

    // Get breakpoints array — find the array, parse elements
    m_debugger.ClearBreakpoints(); // DAP: replace all breakpoints for this source

    // Find "breakpoints" array and parse line numbers
    auto bpPos = args.find("\"breakpoints\"");
    if (bpPos != std::string::npos)
    {
        auto arrStart = args.find('[', bpPos);
        if (arrStart != std::string::npos)
        {
            // Parse each {"line": N} in the array
            size_t pos = arrStart + 1;
            while (pos < args.size())
            {
                auto objStart = args.find('{', pos);
                if (objStart == std::string::npos || objStart > args.find(']', pos)) break;
                auto objEnd = args.find('}', objStart);
                if (objEnd == std::string::npos) break;
                std::string bpObj = args.substr(objStart, objEnd - objStart + 1);
                int lineNo = jsonGetInt(bpObj, "line", -1);
                if (lineNo > 0)
                    m_debugger.AddBreakpoint(path, lineNo);
                pos = objEnd + 1;
            }
        }
    }

    // Build response with actual breakpoints
    std::ostringstream bps;
    bps << "\"breakpoints\":[";
    // We could list the breakpoints we set, but for MVP just echo back
    bps << "]";

    sendResponse(requestSeq, "setBreakpoints", true, bps.str());
}

void DAPServer::onConfigurationDone(int requestSeq)
{
    sendResponse(requestSeq, "configurationDone", true);

    // The script is already running; if paused (e.g. by RequestPause in launch),
    // resume now. Otherwise just wait for a breakpoint.
    waitForStop();
}

void DAPServer::onThreads(int requestSeq)
{
    sendResponse(requestSeq, "threads", true,
        "\"threads\":[{\"id\":1,\"name\":\"main\"}]");
}

void DAPServer::onStackTrace(int requestSeq, const std::string& args)
{
    int startFrame = jsonGetInt(args, "startFrame", 0);
    int levels = jsonGetInt(args, "levels", 20);

    auto frames = m_debugger.GetStackFrames();

    std::ostringstream sf;
    sf << "\"stackFrames\":[";
    for (size_t i = startFrame; i < frames.size() && i < (size_t)(startFrame + levels); i++)
    {
        if (i > (size_t)startFrame) sf << ",";
        sf << "{"
           << "\"id\":" << i << ","
           << "\"name\":\"" << jsonEscape(frames[i].functionName) << "\","
           << "\"source\":{\"name\":\"" << jsonEscape(frames[i].filename) << "\","
           << "\"path\":\"" << jsonEscape(frames[i].filename) << "\"},"
           << "\"line\":" << frames[i].line << ","
           << "\"column\":" << frames[i].column << ""
           << "}";
    }
    sf << "],\"totalFrames\":" << frames.size();

    sendResponse(requestSeq, "stackTrace", true, sf.str());
}

void DAPServer::onScopes(int requestSeq, const std::string& /*args*/)
{
    sendResponse(requestSeq, "scopes", true,
        "\"scopes\":["
        "{\"name\":\"Local\",\"variablesReference\":1,\"expensive\":false}"
        "]");
}

void DAPServer::onVariables(int requestSeq, const std::string& args)
{
    int varRef = jsonGetInt(args, "variablesReference", 1);

    // varRef 1 = frame 0 locals; varRef 2 = frame 1 locals; etc.
    int frameIdx = varRef - 1;
    if (frameIdx < 0) frameIdx = 0;

    auto vars = GetFrameVariables(m_ctx, frameIdx);

    std::ostringstream vv;
    vv << "\"variables\":[";
    for (size_t i = 0; i < vars.size(); i++)
    {
        if (i > 0) vv << ",";
        vv << "{"
           << "\"name\":\"" << jsonEscape(vars[i].name) << "\","
           << "\"value\":\"" << jsonEscape(vars[i].value) << "\","
           << "\"variablesReference\":0"
           << "}";
    }
    vv << "]";

    sendResponse(requestSeq, "variables", true, vv.str());
}

void DAPServer::onContinue(int requestSeq)
{
    sendResponse(requestSeq, "continue", true);
    waitForStop();
}

void DAPServer::onPause(int requestSeq)
{
    m_debugger.RequestPause();
    sendResponse(requestSeq, "pause", true);
}

void DAPServer::onNext(int requestSeq)
{
    auto frames = m_debugger.GetStackFrames();
    if (!frames.empty())
        m_debugger.StepOver(frames[0].filename, frames[0].line, frames[0].pc, (int)frames.size());
    sendResponse(requestSeq, "next", true);
    waitForStop();
}

void DAPServer::onStepIn(int requestSeq)
{
    auto frames = m_debugger.GetStackFrames();
    if (!frames.empty())
        m_debugger.StepInto(frames[0].filename, frames[0].line, frames[0].pc, (int)frames.size());
    sendResponse(requestSeq, "stepIn", true);
    waitForStop();
}

void DAPServer::onStepOut(int requestSeq)
{
    auto frames = m_debugger.GetStackFrames();
    if (!frames.empty())
    {
        m_debugger.ClearBreakpoints();
        m_debugger.StepOut((int)frames.size());
    }
    sendResponse(requestSeq, "stepOut", true);
    waitForStop();
}

void DAPServer::onDisconnect(int requestSeq)
{
    sendResponse(requestSeq, "disconnect", true);
    m_running = false;
}

// ---------- Script execution ----------

void DAPServer::startScript()
{
    if (m_launchScript.empty()) return;

    m_debugger.RequestPause(); // pause on entry

    m_jsThread = std::thread([this]() {
        JSValue result = JS_Eval(
            m_ctx, m_launchScript.c_str(), m_launchScript.size(),
            m_scriptFile.c_str(), JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(result))
        {
            JSValue exc = JS_GetException(m_ctx);
            const char* err = JS_ToCString(m_ctx, exc);
            if (err) { std::cerr << err << std::endl; JS_FreeCString(m_ctx, err); }
            JS_FreeValue(m_ctx, exc);
        }
        JS_FreeValue(m_ctx, result);
        m_debugger.SignalFinished();
    });
}

void DAPServer::waitForStop()
{
    m_debugger.Resume();
    m_debugger.WaitForPauseOrFinish();

    if (m_debugger.IsFinished())
    {
        if (m_jsThread.joinable())
            m_jsThread.join();
        sendEvent("terminated");
        m_running = false;
        return;
    }

    // Determine stop reason
    std::string reason = "breakpoint";
    std::string desc;

    if (m_debugger.IsExceptionPauseEnabled())
    {
        // Check if stopped in a throw context — for MVP just use step/pause detection
    }

    std::ostringstream body;
    body << "\"reason\":\"" << reason << "\","
         << "\"threadId\":1,"
         << "\"allThreadsStopped\":true";
    if (!desc.empty())
        body << ",\"description\":\"" << jsonEscape(desc) << "\"";

    sendEvent("stopped", body.str());
}

// ---------- Main loop ----------

void DAPServer::Run()
{
    if (!m_rt || !m_ctx)
    {
        std::cerr << "DAPServer: not initialized" << std::endl;
        return;
    }

    m_running = true;

    // Reader thread — reads DAP messages from stdin
    std::mutex queueMutex;
    std::condition_variable queueCv;
    std::vector<std::string> messageQueue;

    std::thread reader([&]() {
        while (m_running)
        {
            std::string msg = readMessage();
            if (msg.empty())
            {
                m_running = false;
                queueCv.notify_one();
                break;
            }
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                messageQueue.push_back(msg);
            }
            queueCv.notify_one();
        }
    });

    // Main dispatch loop
    while (m_running)
    {
        std::string msg;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCv.wait(lock, [&]() {
                return !messageQueue.empty() || !m_running;
            });
            if (!m_running) break;
            msg = messageQueue.front();
            messageQueue.erase(messageQueue.begin());
        }

        // Parse DAP message
        std::string type = jsonGetString(msg, "type");
        if (type != "request")
            continue;

        std::string command = jsonGetString(msg, "command");
        int seq = jsonGetInt(msg, "seq", 0);

        // Extract arguments
        std::string args = jsonGetObject(msg, "arguments");

        handleRequest(command, args, seq);
    }

    reader.join();
}
