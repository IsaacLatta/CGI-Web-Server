#pragma once

#include <cstddef>
#include "core/time.h"

namespace logger {

    struct Entry;

    class Logger;

    class Sink;

    constexpr size_t MAX_SINKS = { 1024u };
    constexpr size_t LOG_BUFFER_SIZE { 1024u };
    constexpr size_t ENTRY_BATCH_SIZE { 10u };
    constexpr auto BATCH_TIMEOUT { core::Ms(10u) };

    enum class Level {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Status
    };

}
