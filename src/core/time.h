#pragma once

#include <chrono>
#include <thread>

namespace core {

    using Secs = std::chrono::seconds;
    using Ms = std::chrono::milliseconds;
    using Mins = std::chrono::minutes;
    using Hrs = std::chrono::hours;

    using MonoClock = std::chrono::steady_clock;

    using WallClock = std::chrono::system_clock;

    using WallTimePoint = std::chrono::time_point<WallClock>;

    using MonoTimePoint = std::chrono::time_point<MonoClock>;

    using Duration = MonoClock::duration;

    inline void sleep_ms(uint64_t ms) {
        std::this_thread::sleep_for(Ms(ms));
    }
}