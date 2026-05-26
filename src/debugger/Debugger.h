#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

class Debugger
{
public:
    void RequestPause();

    void Resume();

    bool ShouldPause() const;

    void SuspendVM();

private:
    std::atomic<bool> m_pauseRequested = false;

    std::mutex m_mutex;

    std::condition_variable m_cv;

    bool m_paused = false;
};