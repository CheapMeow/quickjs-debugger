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

class Debugger
{
public:
    void RequestPause();

    void Resume();

    bool ShouldPause() const;

    void SuspendVM(const std::vector<StackFrame>& frames);

    std::vector<StackFrame> GetStackFrames() const;

    StackFrame GetCurrentFrame() const;

private:
    std::atomic<bool> m_pauseRequested{false};

    std::mutex m_mutex;

    std::condition_variable m_cv;

    bool m_paused = false;

    std::vector<StackFrame> m_stackFrames;
};
