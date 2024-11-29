#include <exception>
#include <memory>

#include <fmt/format.h>

#include <sihd/util/Process.hpp>
#include <sihd/util/proc.hpp>

namespace sihd::util::proc
{

#if !defined(__SIHD_WINDOWS__)

namespace
{

void configure_process(std::shared_ptr<Process> & proc_ptr, const Options & options)
{
    if (options.close_stdin && !options.to_stdin.empty())
        throw std::logic_error("Cannot close and send input through stdin");
    if (options.close_stdout && options.stdout_callback)
        throw std::logic_error("Cannot close and pipe stdout");
    if (options.close_stderr && options.stderr_callback)
        throw std::logic_error("Cannot close and pipe stderr");

    if (!options.to_stdin.empty())
    {
        proc_ptr->stdin_from(options.to_stdin);
        proc_ptr->stdin_close_after_exec(true);
    }
    if (options.stdout_callback)
        proc_ptr->stdout_to(options.stdout_callback);
    if (options.stderr_callback)
        proc_ptr->stderr_to(options.stderr_callback);

    if (options.close_stdin)
        proc_ptr->stdin_close();
    if (options.close_stdout)
        proc_ptr->stdout_close();
    if (options.close_stderr)
        proc_ptr->stderr_close();

    if (options.purge_env)
        proc_ptr->environ_clear();
    if (!options.env.empty())
        proc_ptr->environ_load(options.env);
}

int do_execute(std::shared_ptr<Process> proc_ptr, Timestamp max_timeout)
{
    SteadyClock clock;
    constexpr int poll_timeout_ms = 10;

    // if process failed to execute return -1
    if (!proc_ptr->execute())
        return static_cast<int>(static_cast<unsigned char>(-1));

    const bool has_to_poll = proc_ptr->can_read_pipes();
    bool timed_out = false;
    time_t begin = clock.now();
    while (!timed_out)
    {
        // ensure reading the stdout/stderr
        if (has_to_poll)
            proc_ptr->read_pipes(poll_timeout_ms);
        // check the process status - break the loop if exited
        proc_ptr->wait_any(WNOHANG);
        if (proc_ptr->has_exited())
            break;
        // if there is no polling then there is no sleep - be easy in the processor
        if (has_to_poll == false)
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_timeout_ms));
        // check if the timeout is reached
        if (max_timeout > 0)
            timed_out = (clock.now() - begin) >= max_timeout;
    }
    fmt::print("timeout: {}\n", timed_out);
    proc_ptr->terminate();
    return static_cast<int>(proc_ptr->return_code());
}

} // namespace

std::future<int> execute(const std::vector<std::string> & args, const Options & options)
{
    std::shared_ptr<Process> proc_ptr = std::make_shared<Process>(args);

    configure_process(proc_ptr, options);

    std::future<int> exit_code = std::async(std::launch::async, [proc_ptr, timeout = options.timeout] {
        return do_execute(proc_ptr, timeout);
    });

    return exit_code;
}

#else

std::future<int> execute([[maybe_unused]] const std::vector<std::string> & args,
                         [[maybe_unused]] const Options & options)
{
    return {};
}

#endif

} // namespace sihd::util::proc
