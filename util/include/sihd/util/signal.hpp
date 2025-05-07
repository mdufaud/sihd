#ifndef __SIHD_UTIL_SIGNAL_HPP__
#define __SIHD_UTIL_SIGNAL_HPP__

#include <optional>
#include <string>

#include <sihd/util/Timestamp.hpp>

namespace sihd::util::signal
{

struct SigStatus
{
        size_t received = 0;
        sihd::util::Timestamp time_received = -1;

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
bool handle(int sig);

// set signal handling to default
bool unhandle(int sig);

bool ignore(int sig);

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

bool kill(pid_t pid, int sig);

std::string name(int sig);

std::string status_str();

} // namespace sihd::util::signal

#endif
