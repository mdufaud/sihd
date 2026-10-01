#ifndef __SIHD_SYS_SIGNAL_HPP__
#define __SIHD_SYS_SIGNAL_HPP__

#include <sys/types.h>

#include <atomic>
#include <expected>
#include <optional>
#include <span>
#include <string>

#include <sihd/util/Error.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::sys::signal
{

inline constexpr int max_signal = 64;

struct SigStatus
{
        std::atomic<size_t> received = 0;
        std::atomic<sihd::util::Timestamp> time_received {sihd::util::Timestamp(-1)};

        SigStatus();
        ~SigStatus();
        SigStatus(const SigStatus & other);
        SigStatus & operator=(const SigStatus & other);

        operator bool() const;
};

struct SigExitConfig
{
        bool on_stop = false;
        bool on_termination = false;
        bool on_dump = false;
        bool log_signal = false;
        bool exit_with_sig_number = false;
};
void set_exit_config(const SigExitConfig & config);

// set signal handling to handled
std::expected<void, sihd::util::Error> handle(int sig);

// set signal handling to default
std::expected<void, sihd::util::Error> unhandle(int sig);

std::expected<void, sihd::util::Error> ignore(int sig);

bool is_category_stop(int sig);
bool is_category_termination(int sig);
bool is_category_dump(int sig);

int last_received();
std::optional<SigStatus> status(int sig);

bool stop_received();
bool termination_received();
bool dump_received();

bool should_stop();
void reset_received(int sig);
void reset_all_received();

#if !defined(__SIHD_WINDOWS__)
std::expected<void, sihd::util::Error> block_thread(int sig);
std::expected<void, sihd::util::Error> block_thread(std::span<const int> sigs);
std::expected<void, sihd::util::Error> unblock_thread(int sig);
std::expected<void, sihd::util::Error> unblock_thread(std::span<const int> sigs);
#endif

std::expected<void, sihd::util::Error> kill(pid_t pid, int sig);

std::string name(int sig);

std::string status_str();

} // namespace sihd::sys::signal

#endif
