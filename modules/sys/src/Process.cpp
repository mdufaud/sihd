#include <cerrno>
#include <cstdio>
#include <stdexcept>

#include <sihd/sys/Process.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/str.hpp>

// everything touching child process spawning, pipes and the platform watchers lives in
// src/linux|windows/Process.cpp

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

auto get_in_env(const std::vector<std::string> & env, std::string_view key)
{
    return container::find_if(env,
                              [&key](const std::string & env) { return str::starts_with(env, key, "="); });
}

template <typename T>
void env_load_impl(const T & to_load_environ, std::vector<std::string> & environment)
{
    for (const auto & env : to_load_environ)
    {
        auto [key, value] = str::split_pair_view(env, "=");
        if (key.empty())
            continue;

        auto it = get_in_env(environment, key);
        if (it != environment.end())
            environment.erase(it);
        environment.emplace_back(env);
    }
}

} // namespace

void Process::env_clear()
{
    _environment.clear();
}

void Process::env_load(std::span<const std::string> to_load_environ)
{
    env_load_impl(to_load_environ, _environment);
}

void Process::env_load(std::span<const char *> to_load_environ)
{
    env_load_impl(to_load_environ, _environment);
}

void Process::env_load(std::span<std::string_view> to_load_environ)
{
    env_load_impl(to_load_environ, _environment);
}

void Process::env_load(std::initializer_list<std::string_view> to_load_environ)
{
    env_load_impl(to_load_environ, _environment);
}

void Process::env_set(std::string_view key, std::string_view value)
{
    std::string keyval = fmt::format("{}={}", key, value);
    for (std::string & env : _environment)
    {
        if (str::starts_with(env, key, "="))
        {
            env = std::move(keyval);
            return;
        }
    }
    _environment.emplace_back(std::move(keyval));
}

std::optional<std::string> Process::env_get(std::string_view key) const
{
    const auto it = get_in_env(_environment, key);
    if (it != _environment.end())
    {
        auto [_, value] = str::split_pair_view(*it, "=");
        return std::string(value);
    }
    return std::nullopt;
}

bool Process::env_rm(std::string_view key)
{
    auto it = get_in_env(_environment, key);
    if (it != _environment.end())
    {
        _environment.erase(it);
    }
    return it != _environment.end();
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
