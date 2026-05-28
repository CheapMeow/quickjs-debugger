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

    m_cv.notify_all();

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

void Debugger::AddBreakpoint(const std::string& filename, int line)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& bp : m_breakpoints)
    {
        if (bp.filename == filename && bp.line == line)
            return;
    }

    m_breakpoints.push_back({filename, line});

    std::cout << "Breakpoint set at " << filename << ":" << line << "\n";
}

void Debugger::RemoveBreakpoint(const std::string& filename, int line)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_breakpoints.erase(
        std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
            [&](const Breakpoint& bp) {
                return bp.filename == filename && bp.line == line;
            }),
        m_breakpoints.end());
}

void Debugger::ClearBreakpoints()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_breakpoints.clear();
}

bool Debugger::ShouldBreak(const std::string& filename, int line) const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));

    for (const auto& bp : m_breakpoints)
    {
        if (bp.filename == filename && bp.line == line)
            return true;
    }

    return false;
}

bool Debugger::IsPaused() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));

    return m_paused;
}

void Debugger::WaitUntilPaused()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_cv.wait(lock, [&]() { return m_paused; });
}

void Debugger::SetExceptionPauseEnabled(bool enabled)
{
    m_pauseOnException.store(enabled);
}

bool Debugger::IsExceptionPauseEnabled() const
{
    return m_pauseOnException.load();
}

void Debugger::StepInto(const std::string& curFile, int curLine, uint32_t curPc, int frameDepth)
{
    std::lock_guard<std::mutex> lock(m_stepMutex);

    m_stepFile = curFile;
    m_stepLine = curLine;
    m_stepPc = curPc;
    m_stepDepth.store(frameDepth);
    m_stepMode.store(static_cast<int>(StepMode::StepInto));
}

void Debugger::StepOver(const std::string& curFile, int curLine, uint32_t curPc, int frameDepth)
{
    std::lock_guard<std::mutex> lock(m_stepMutex);

    m_stepFile = curFile;
    m_stepLine = curLine;
    m_stepPc = curPc;
    m_stepDepth.store(frameDepth);
    m_stepMode.store(static_cast<int>(StepMode::StepOver));
}

void Debugger::StepOut(int frameDepth)
{
    std::lock_guard<std::mutex> lock(m_stepMutex);

    m_stepDepth.store(frameDepth);
    m_stepMode.store(static_cast<int>(StepMode::StepOut));
}

bool Debugger::ShouldStep(const std::string& filename, int line, uint32_t pc, int frameDepth)
{
    int mode = m_stepMode.load();

    if (mode == static_cast<int>(StepMode::None))
        return false;

    if (mode == static_cast<int>(StepMode::StepInto))
    {
        // Pause when PC changes (even within the same line)
        std::lock_guard<std::mutex> lock(m_stepMutex);

        if (pc != m_stepPc)
        {
            m_stepMode.store(static_cast<int>(StepMode::None));

            return true;
        }

        return false;
    }

    if (mode == static_cast<int>(StepMode::StepOver))
    {
        std::lock_guard<std::mutex> lock(m_stepMutex);

        // Pause when back at same depth (or shallower) at a new PC
        if (frameDepth <= m_stepDepth.load())
        {
            if (pc != m_stepPc)
            {
                m_stepMode.store(static_cast<int>(StepMode::None));

                return true;
            }
        }

        return false;
    }

    if (mode == static_cast<int>(StepMode::StepOut))
    {
        // Pause when depth decreases (we returned from current function)
        if (frameDepth < m_stepDepth.load())
        {
            m_stepMode.store(static_cast<int>(StepMode::None));

            return true;
        }

        return false;
    }

    return false;
}
