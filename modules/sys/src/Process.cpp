#include <cerrno>
#include <cstdio>
#include <stdexcept>

#include <sihd/sys/Process.hpp>
#include <sihd/util/Logger.hpp>

// everything touching child process spawning, pipes and the platform watchers lives in
// src/linux|windows/Process.cpp

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

std::vector<std::string> Process::env() const
{
    std::vector<std::string> entries;
    entries.reserve(_env.size());
    for (const auto & [key, value] : _env.entries())
        entries.emplace_back(fmt::format("{}={}", key, value));
    return entries;
}

void Process::env_clear()
{
    _env.clear();
}

void Process::env_load(std::span<const std::string> to_load_environ)
{
    _env.load(to_load_environ);
}

void Process::env_load(std::span<const char *> to_load_environ)
{
    _env.load(to_load_environ);
}

void Process::env_load(std::span<std::string_view> to_load_environ)
{
    _env.load(to_load_environ);
}

void Process::env_load(std::initializer_list<std::string_view> to_load_environ)
{
    _env.load(to_load_environ);
}

void Process::env_set(std::string_view key, std::string_view value)
{
    _env.set(key, value);
}

std::optional<std::string> Process::env_get(std::string_view key) const
{
    return _env.get(key);
}

bool Process::env_rm(std::string_view key)
{
    return _env.rm(key);
}

void Process::set_chdir(std::string_view path)
{
    _chdir = path;
}

// Argv

Process & Process::set_function(std::nullptr_t)
{
    _fun_to_execute = nullptr;
    return *this;
}

Process & Process::set_function(std::function<int()> fun)
{
    _fun_to_execute = std::move(fun);
    return *this;
}

void Process::clear_argv()
{
    _argv.clear();
}

Process & Process::add_argv(std::string_view arg)
{
    _argv.emplace_back(arg);
    return *this;
}

Process & Process::add_argv(const std::vector<std::string> & args)
{
    _argv.insert(_argv.end(), args.begin(), args.end());
    return *this;
}

Process & Process::stdin_close_after_exec(bool activate)
{
    _close_stdin_after_exec = activate;
    return *this;
}

} // namespace sihd::sys
