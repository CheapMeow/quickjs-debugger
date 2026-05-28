#include "Debugger.h"

#include <iostream>

void Debugger::RequestPause()
{
    m_pauseRequested.store(true);
}

void Debugger::Resume()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_paused = false;

        m_pauseRequested.store(false);
    }

    m_cv.notify_all();
}

bool Debugger::ShouldPause() const
{
    return m_pauseRequested.load();
}

void Debugger::SuspendVM(const std::vector<StackFrame>& frames)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_paused = true;

    m_stackFrames = frames;

    std::cout << "\n=== JS VM PAUSED ===\n";

    if (!m_stackFrames.empty())
    {
        const auto& top = m_stackFrames[0];

        std::cout << "Location: " << top.filename
                  << ":" << top.line
                  << ":" << top.column
                  << "  [" << top.functionName << "]\n";
    }

    m_cv.wait(lock, [&]() { return !m_paused; });

    std::cout << "\n=== JS VM RESUMED ===\n";
}

std::vector<StackFrame> Debugger::GetStackFrames() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));

    return m_stackFrames;
}

StackFrame Debugger::GetCurrentFrame() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));

    if (m_stackFrames.empty())
        return {};

    return m_stackFrames[0];
}
