#include "Debugger.h"

#include <iostream>

void Debugger::RequestPause() { m_pauseRequested.store(true); }

void Debugger::Resume()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_paused = false;

        m_pauseRequested.store(false);
    }

    m_cv.notify_all();
}

bool Debugger::ShouldPause() const { return m_pauseRequested.load(); }

void Debugger::SuspendVM()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_paused = true;

    std::cout << "\n=== JS VM PAUSED ===\n";

    m_cv.wait(lock, [&]() { return !m_paused; });

    std::cout << "\n=== JS VM RESUMED ===\n";
}