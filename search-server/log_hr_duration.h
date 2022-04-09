#pragma once

#include <chrono>
#include <vector>

class LogHighResulutionDuration {
public:
    LogHighResulutionDuration(int slots = 10);
    ~LogHighResulutionDuration();

    void Start(int slot);
    void Stop(int slot);
    void Print();

private:
    int slots_;
    std::vector<std::chrono::high_resolution_clock::time_point> start_;
    // std::vector<std::chrono::high_resolution_clock::duration> duration_;
    std::vector<std::chrono::duration<double>> duration_;
};

// extern LogHighResulutionDuration global_hr_duration;

class LogHighResolutionSlot {
public:
    LogHighResolutionSlot(LogHighResulutionDuration& hr, int slot);
    ~LogHighResolutionSlot();
private:
    LogHighResulutionDuration& hr_;
    int slot_;
};


