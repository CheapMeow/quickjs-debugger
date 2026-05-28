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

void Debugger::SuspendVM(const JSLocation& location)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_paused = true;

    m_currentLocation = location;

    std::cout << "\n=== JS VM PAUSED ===\n";
    std::cout << "Location: " << location.filename
              << ":" << location.line
              << ":" << location.column << "\n";

    m_cv.wait(lock, [&]() { return !m_paused; });

    std::cout << "\n=== JS VM RESUMED ===\n";
}

JSLocation Debugger::GetCurrentLocation() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));

    return m_currentLocation;
}