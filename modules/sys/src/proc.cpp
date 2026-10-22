#include <exception>
#include <memory>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/proc.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/thread.hpp>
#include <sihd/util/time.hpp>

namespace sihd::sys
{

SIHD_LOGGER;
}

namespace sihd::sys::proc
{

using namespace sihd::util;

namespace
{

void configure_process(Process & proc, const Options & options)
{
    if (options.close_stdin && !options.to_stdin.empty())
        throw std::logic_error("Cannot close and send input through stdin");
    if (options.close_stdout && options.stdout_callback)
        throw std::logic_error("Cannot close and pipe stdout");
    if (options.close_stderr && options.stderr_callback)
        throw std::logic_error("Cannot close and pipe stderr");

    if (!options.to_stdin.empty())
    {
        proc.stdin_from(options.to_stdin);
        proc.stdin_close_after_exec(true);
    }
    if (options.stdout_callback)
        proc.stdout_to(options.stdout_callback);
    if (options.stderr_callback)
        proc.stderr_to(options.stderr_callback);

    if (options.close_stdin)
        proc.stdin_close();
    if (options.close_stdout)
        proc.stdout_close();
    if (options.close_stderr)
        proc.stderr_close();

    if (options.purge_env)
        proc.env_clear();
    if (!options.env.empty())
        proc.env_load(options.env);

    if (options.nice.has_value())
        proc.set_nice(*options.nice);
    if (options.scheduler_policy.has_value())
        proc.set_scheduler(*options.scheduler_policy, options.scheduler_priority);
    if (options.affinity.empty() == false)
        proc.set_affinity(options.affinity);
}

int do_execute(Process & proc, Duration max_timeout)
{
    constexpr int poll_timeout_ms = 10;
    SteadyClock clock;

    auto executed = proc.execute();
    if (SIHD_UNEXPECTED_LOG(executed))
        return Process::failure_return_code;

    const bool has_to_poll = proc.can_read_pipes();
    bool timed_out = false;
    Timestamp begin = clock.now();
    while (!timed_out)
    {
        if (has_to_poll)
        {
            proc.read_pipes(poll_timeout_ms);
        }
#if defined(WNOHANG)
        (void)proc.wait_any(WNOHANG);
#else
        (void)proc.wait(poll_timeout_ms);
#endif
        if (proc.has_terminated())
        {
            break;
        }
        if (has_to_poll == false)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_timeout_ms));
        }
        if (max_timeout > 0)
            timed_out = (clock.now() - begin) >= max_timeout;
    }
    proc.terminate();
    return static_cast<int>(proc.return_code());
}

template <typename T>
std::future<int> execute_impl(const T & args, const Options & options)
{
    auto proc = std::make_unique<Process>(args);

    configure_process(*proc, options);

    std::future<int> exit_code = std::async(std::launch::async, [proc = std::move(proc), timeout = options.timeout] {
        SIHD_UNEXPECTED_LOG(thread::set_name("proc::execute"));
        return do_execute(*proc, timeout);
    });

    return exit_code;
}

} // namespace

std::future<int> execute(std::span<const std::string> args, const Options & options)
{
    return execute_impl(args, options);
}

std::future<int> execute(std::span<std::string_view> args, const Options & options)
{
    return execute_impl(args, options);
}

std::future<int> execute(std::span<const char *> args, const Options & options)
{
    return execute_impl(args, options);
}

std::future<int> execute(std::initializer_list<std::string_view> args, const Options & options)
{
    return execute_impl(args, options);
}

} // namespace sihd::sys::proc
