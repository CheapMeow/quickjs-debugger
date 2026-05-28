#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

struct JSLocation
{
    std::string filename;
    int line = -1;
    int column = -1;
};

class Debugger
{
public:
    void RequestPause();

    void Resume();

    bool ShouldPause() const;

    void SuspendVM(const JSLocation& location);

    JSLocation GetCurrentLocation() const;

private:
    std::atomic<bool> m_pauseRequested = false;

    std::mutex m_mutex;

    std::condition_variable m_cv;

    bool m_paused = false;

    JSLocation m_currentLocation;
};