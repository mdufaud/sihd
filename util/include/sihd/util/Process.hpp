#ifndef __SIHD_UTIL_PROCESS_HPP__
# define __SIHD_UTIL_PROCESS_HPP__

# include <sihd/util/Str.hpp>
# include <sihd/util/IRunnable.hpp>
# include <sihd/util/platform.hpp>
# include <functional>
# include <optional>

namespace sihd::util
{

# if !defined(__SIHD_WINDOWS__)

class Process: virtual public IRunnable
{
    public:
        Process(const std::vector<std::string> & cmd);
        Process(std::initializer_list<const char *> cmd);
        virtual ~Process();

        bool run();

        /*
        Process & stdin_from(const std::string & input);
        Process & stdin_from(int fd);
        Process & stdin_from_file(const std::string & path);

        Process & stdout_to(std::function<bool(const std::string &)> fun);
        Process & stdout_to(std::string & output);
        Process & stdout_to(int fd);
        Process & stdout_to_file(const std::string & path);

        Process & stderr_to(std::function<bool(const std::string &)> fun);
        Process & stderr_to(std::string & output);
        Process & stderr_to(int fd);
        Process & stderr_to_file(const std::string & path);

        Process & add_arg(const std::string & arg);
        Process & add_arg(const std::vector<std::string> & args);
        */

        std::optional<bool> has_stopped();
        std::optional<bool> has_exited();
        std::optional<bool> has_core_dumped();
        std::optional<bool> has_exited_by_signal();
        std::optional<int>  signal_exit_number();
        std::optional<int>  signal_stop_number();
        std::optional<int>  return_code();

        std::optional<int> wait();
        
        bool has_run();

    private:
        void _init();
        void _dup_close(int fd_from, int fd_to);
        void _close(int fd);
        std::pair<int, int> _pipe();

        int _pid;
        int _stdin_fd;
        int _stdout_fd;
        int _stderr_fd;
        std::optional<int> _status;
        std::vector<const char *> _cmd;
};

# else

class Process
{
    public:
        Process() {};
        virtual ~Process() {};
};

# endif

}

#endif 