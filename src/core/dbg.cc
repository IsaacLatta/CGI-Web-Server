#include "core/dbg.h"

#include <atomic>
#include <csignal>
#include <fstream>
#include <sys/types.h>
#include <exception>

namespace {

    void try_print_assertion(const core::dbg::Assertion& assertion) {
        try {
            const auto assertion_msg = to_string(assertion);
            core::dbg::print_to_stderr(assertion_msg.c_str());
        } catch (const std::exception& e) {
            core::dbg::print_to_stderr(e.what());
        }
    }

#if DEBUG_MODE
    std::atomic<int> s_disable_depth { 0 };

    void add_depth() {
        s_disable_depth.fetch_add(1, std::memory_order_acq_rel);
    }

    void remove_depth() {
        s_disable_depth.fetch_sub(1, std::memory_order_acq_rel);
    }
#endif

}

namespace core::dbg {

    bool is_disabled() noexcept {
#if DEBUG_MODE
        return s_disable_depth.load(std::memory_order_acquire) > 0;
#else
        return false;
#endif
    }

#if DEBUG_MODE
    AssertionGuard::AssertionGuard() {
        add_depth();
    }

    AssertionGuard::~AssertionGuard() {
        remove_depth();
    }
#else
    AssertionGuard::AssertionGuard() {}
    AssertionGuard::~AssertionGuard() {}
#endif

    [[noreturn]] void on_unreachable(const Assertion& assertion) noexcept {
        try_print_assertion(assertion);

#if DEBUG_MODE
        if (is_disabled()) {
            print_to_stderr("unreachable reached while assertions are disabled; aborting anyway");
            abort_here();
        }

        if (is_debugger_present()) {
            break_here();
        }

        abort_here();
#else
        abort_here();
#endif
    }

    void on_assertion(const Assertion& assertion) noexcept {
        try_print_assertion(assertion);

#if DEBUG_MODE
        if (is_disabled()) {
            return;
        }

        if (is_debugger_present()) {
            break_here();
            return;
        }

        abort_here();
#else
        abort_here();
#endif

    }

    void abort_here() noexcept {
        std::abort();
    }

    void break_here() noexcept {
        std::raise(SIGTRAP);
    }

    bool is_debugger_present() noexcept {
        const std::string target { "TracerPid:" };
        std::ifstream file("/proc/self/status");
        std::string line;
        while (std::getline(file, line)) {
            if (line.find(target, 0) == 0) {
                std::string str = line.substr(target.length());
                return std::stol(str) != 0;
            }
        }
        return false;
    }

}