#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <source_location>
#include <cassert>
#include <thread>

#include <fmt/format.h>

#include "core/time.h"

namespace core::dbg {

    class AssertionGuard {
    public:
        AssertionGuard();
        ~AssertionGuard();

        AssertionGuard(AssertionGuard&&) = delete;
        AssertionGuard& operator=(AssertionGuard&&) = delete;
        AssertionGuard(const AssertionGuard&) = delete;
        AssertionGuard& operator=(const AssertionGuard&) = delete;
    };

    struct Assertion {
        std::string message;
        std::source_location location{};
        uint64_t call_id { 0u };
        WallTimePoint when{};
    };

    inline std::string to_string(const Assertion& assertion) {
        return fmt::format(
            "assertion call_id={} file={} line={} col={} func={} msg=\"{}\"",
            assertion.call_id,
            assertion.location.file_name(),
            assertion.location.line(),
            assertion.location.column(),
            assertion.location.function_name(),
            assertion.message
        );
    }

    inline void print_to_stderr(const char* msg) noexcept {
        if (!msg) {
            msg = "<null msg>";
        }
        constexpr size_t bytes_per_char { 1 };
        (void)std::fwrite(msg, bytes_per_char, std::strlen(msg), stderr);
        (void)std::fwrite("\n", bytes_per_char, bytes_per_char, stderr);
        (void)std::fflush(stderr);
    }

    bool is_disabled() noexcept;
    void on_assertion(const Assertion&) noexcept;
    bool is_debugger_present() noexcept;

    void break_here() noexcept;
    [[noreturn]] void abort_here() noexcept;
    [[noreturn]] void on_unreachable(const Assertion&) noexcept;
}

#define DO_CONCAT(a,b) a##b
#define CONCAT(a,b) DO_CONCAT(a,b)
#define UNIQUE_NAME(a) CONCAT(a, __COUNTER__)

#define DBG_NEXT_CALL_ID() \
    ([&]() -> uint64_t { \
        static std::atomic<uint64_t> dbg_call_id_hits { 0 }; \
        return 1u + dbg_call_id_hits.fetch_add(1, std::memory_order_relaxed); \
    }())

#define DBG_MAKE_ASSERTION(msg, cid) \
    ([&]() -> core::dbg::Assertion { \
        return core::dbg::Assertion { \
            .message = (msg), \
            .location = std::source_location::current(), \
            .call_id = static_cast<uint64_t>(cid), \
            .when = core::WallClock::now(), \
        }; \
    }())

#define DBG_CHECK_IMPL(cond, ...) do { \
    const uint64_t __cid = DBG_NEXT_CALL_ID(); \
    if (!(cond)) { \
        auto dbg_msg = fmt::format("assert {} | {}", #cond, fmt::format(__VA_ARGS__)); \
        core::dbg::on_assertion(DBG_MAKE_ASSERTION(dbg_msg, __cid)); \
    } \
} while (0)

#define DO_DBG_ASSERT(cond, ...) \
    DBG_CHECK_IMPL(cond, __VA_ARGS__)

#define DBG_UNREACHABLE(...) do { \
        const uint64_t dbg_cid = DBG_NEXT_CALL_ID(); \
        auto dbg_msg = fmt::format("unreachable | {}", fmt::format(__VA_ARGS__)); \
        core::dbg::unreachable_here(DBG_MAKE_ASSERTION(dbg_msg, dbg_cid)); \
    } while (0)

#define DBG_ASSERT(cond, ...) \
    DO_DBG_ASSERT(cond, __VA_ARGS__)


