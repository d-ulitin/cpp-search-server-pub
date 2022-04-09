#include "log_hr_duration.h"

#include <iostream>

using namespace std;

// LogHighResulutionDuration global_hr_duration;

LogHighResulutionDuration::LogHighResulutionDuration(int slots) :
    slots_(slots),
    start_(slots, std::chrono::high_resolution_clock::time_point{}),
    duration_(slots, std::chrono::high_resolution_clock::duration{}) {
}

LogHighResulutionDuration::~LogHighResulutionDuration() {
    Print();
}

void LogHighResulutionDuration::Start(int slot) {
    start_[slot] = std::chrono::high_resolution_clock::now();
}

void LogHighResulutionDuration::Stop(int slot) {
    duration_[slot] += (std::chrono::high_resolution_clock::now() - start_[slot]);
}

void LogHighResulutionDuration::Print() {
    for (int i = 0; i < slots_; ++i) {
        cout << i << ": " << duration_[i].count() << endl;
    }
}

LogHighResolutionSlot::LogHighResolutionSlot(LogHighResulutionDuration& hr, int slot) :
    hr_(hr), slot_(slot) {
    hr_.Start(slot);
}

LogHighResolutionSlot::~LogHighResolutionSlot() {
    hr_.Stop(slot_);
}
 

