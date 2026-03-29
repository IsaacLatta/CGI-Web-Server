#pragma once

#include <chrono>

namespace core {

    using Secs = std::chrono::seconds;
    using Mins = std::chrono::minutes;
    using Hrs = std::chrono::hours;


    using MonoClock = std::chrono::steady_clock;

    using WallClock = std::chrono::system_clock;

    using WallTimePoint = std::chrono::time_point<WallClock>;

    using MonoTimePoint = std::chrono::time_point<MonoClock>;
}