#ifndef __SIHD_UTIL_PROC_HPP__
#define __SIHD_UTIL_PROC_HPP__

#include <functional>
#include <future>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Timestamp.hpp>

namespace sihd::util::proc
{

struct Options
{
        Timestamp timeout = -1;
        std::string to_stdin = {};
        std::function<void(std::string_view)> stdout_callback = {};
        std::function<void(std::string_view)> stderr_callback = {};
        bool close_stdin = false;
        bool close_stdout = false;
        bool close_stderr = false;
        bool purge_env = false;
        std::vector<std::string> env = {};
};

std::future<int> execute(std::span<const std::string> args, const Options & options = {});
std::future<int> execute(std::span<std::string_view> args, const Options & options = {});
std::future<int> execute(std::span<const char *> args, const Options & options = {});
std::future<int> execute(std::initializer_list<std::string_view> args, const Options & options = {});

} // namespace sihd::util::proc

#endif
