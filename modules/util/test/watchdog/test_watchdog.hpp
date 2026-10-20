#ifndef __SIHD_UTIL_TEST_WATCHDOG_HPP__
#define __SIHD_UTIL_TEST_WATCHDOG_HPP__

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <sihd/util/Waitable.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <pthread.h>
# include <signal.h>
#else
# include <windows.h>
#endif

#include <gtest/gtest.h>

namespace test
{

// aborts the binary when one test runs past the limit, naming it: a hang shows which test it is
class TestWatchdog: public ::testing::EmptyTestEventListener
{
    public:
        explicit TestWatchdog(std::chrono::seconds limit): _limit(limit)
        {
#if !defined(__SIHD_WINDOWS__)
            // created with every signal blocked: a signal a test waits for must not land on this thread
            sigset_t all;
            sigset_t previous;
            sigfillset(&all);
            pthread_sigmask(SIG_SETMASK, &all, &previous);
            _thread = std::thread([this] { this->watch(); });
            pthread_sigmask(SIG_SETMASK, &previous, nullptr);
#else
            _thread = std::thread([this] { this->watch(); });
#endif
        }

        ~TestWatchdog() override { this->stop(); }

        TestWatchdog(const TestWatchdog &) = delete;
        TestWatchdog & operator=(const TestWatchdog &) = delete;

        void OnTestStart(const ::testing::TestInfo & info) override
        {
            {
                auto lock = _waitable.guard();
                _name = std::string(info.test_suite_name()) + "." + info.name();
                _deadline = std::chrono::steady_clock::now() + _limit;
                _armed = true;
                ++_generation;
            }
            _waitable.notify_all();
        }

        void OnTestEnd(const ::testing::TestInfo &) override
        {
            {
                auto lock = _waitable.guard();
                _armed = false;
                ++_generation;
            }
            _waitable.notify_all();
        }

        // joined before main returns: windows deletes listeners while unloading the gtest dll, where a join deadlocks
        void OnTestProgramEnd(const ::testing::UnitTest &) override { this->stop(); }

    private:
        void stop()
        {
            {
                auto lock = _waitable.guard();
                _stop = true;
            }
            _waitable.notify_all();
            if (_thread.joinable())
                _thread.join();
        }

        void watch()
        {
            while (true)
            {
                uint64_t generation = 0;
                std::chrono::steady_clock::time_point deadline;
                {
                    auto lock = _waitable.wait_guard([this] { return _stop || _armed; });
                    if (_stop)
                        return;
                    generation = _generation;
                    deadline = _deadline;
                }
                const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    deadline - std::chrono::steady_clock::now());
                auto waited = _waitable.wait_for_guard(sihd::util::Duration(remaining.count()), [this, generation] {
                    return _stop || _generation != generation;
                });
                if (waited.timed_out)
                    this->fire();
            }
        }

        void fire()
        {
            std::fprintf(stderr, "\n%s timed out after %llds\n", _name.c_str(), static_cast<long long>(_limit.count()));
            std::fflush(stderr);
#if !defined(__SIHD_WINDOWS__)
            std::abort();
#else
            // abort waits on the crash reporter and _Exit on dll detach: only TerminateProcess stops at once
            TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
#endif
        }

        const std::chrono::seconds _limit;
        sihd::util::Waitable _waitable;
        std::string _name;
        std::chrono::steady_clock::time_point _deadline;
        uint64_t _generation = 0;
        bool _armed = false;
        bool _stop = false;
        std::thread _thread;
};

// below the 2 minutes a test binary gets as a whole: the watchdog names the test before the binary is killed
inline void install_test_watchdog(std::chrono::seconds limit = std::chrono::seconds(60))
{
    ::testing::UnitTest::GetInstance()->listeners().Append(new TestWatchdog(limit));
}

} // namespace test

#endif
