#ifndef __SIHD_SYS_WINDOWS_SCM_HPP__
#define __SIHD_SYS_WINDOWS_SCM_HPP__

#include <cstdint>
#include <memory>
#include <string>

#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# error "sihd/sys/windows headers are windows only"
#endif

namespace sihd::sys::windows_::scm
{

enum class State
{
    stopped,
    start_pending,
    stop_pending,
    running,
    continue_pending,
    pause_pending,
    paused,
};

enum class StartType
{
    auto_start,
    manual,
    disabled,
};

struct Status
{
        State state = State::stopped;
        uint32_t pid = 0;
        uint32_t exit_code = 0;
};

class Service
{
    public:
        Service();
        Service(Service && other);
        Service & operator=(Service && other);
        ~Service();

        Service(const Service &) = delete;
        Service & operator=(const Service &) = delete;

        sihd::util::Result<void> open(const std::string & name, bool control = false);
        sihd::util::Result<void> create(const std::string & name, const std::string & bin_path, StartType start_type);
        void close();

        bool is_open() const;

        // negative timeout waits forever
        sihd::util::Result<void> start(sihd::util::Duration timeout);
        sihd::util::Result<void> stop(sihd::util::Duration timeout);
        sihd::util::Result<void> set_start_type(StartType start_type);
        sihd::util::Result<void> remove();

        sihd::util::Result<Status> status() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys::windows_::scm

#endif
