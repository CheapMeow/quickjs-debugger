#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

struct StackFrame
{
    std::string filename;
    std::string functionName;
    int line = -1;
    int column = -1;
    uint32_t pc = 0;
};

struct Breakpoint
{
    std::string filename;
    int line;
};

enum class StepMode
{
    None,
    StepInto,
    StepOver,
    StepOut,
};

class Debugger
{
public:
    void RequestPause();

    void Resume();

    bool ShouldPause() const;

    void SuspendVM(const std::vector<StackFrame>& frames);

    std::vector<StackFrame> GetStackFrames() const;

    StackFrame GetCurrentFrame() const;

    void AddBreakpoint(const std::string& filename, int line);

    void RemoveBreakpoint(const std::string& filename, int line);

    void ClearBreakpoints();

    bool ShouldBreak(const std::string& filename, int line) const;

    bool IsPaused() const;

    void WaitUntilPaused();

    void SetExceptionPauseEnabled(bool enabled);

    bool IsExceptionPauseEnabled() const;

    // Stepping
    void StepInto(const std::string& curFile, int curLine, uint32_t curPc, int frameDepth);

    void StepOver(const std::string& curFile, int curLine, uint32_t curPc, int frameDepth);

    void StepOut(int frameDepth);

    bool ShouldStep(const std::string& filename, int line, uint32_t pc, int frameDepth);

private:
    std::atomic<bool> m_pauseRequested{false};

    std::mutex m_mutex;

    std::condition_variable m_cv;

    bool m_paused = false;

    std::vector<StackFrame> m_stackFrames;

    std::vector<Breakpoint> m_breakpoints;

    std::atomic<bool> m_pauseOnException{false};

    // Step state (read by interrupt handler, written by main thread)
    std::atomic<int> m_stepMode{0}; // StepMode cast to int

    std::atomic<int> m_stepDepth{0};

    std::string m_stepFile;

    int m_stepLine = -1;

    uint32_t m_stepPc = 0;

    std::mutex m_stepMutex;
};
