#include "internal/environment.hpp"

#include <fmt/format.h>

#include <sihd/sys/env.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys
{

SIHD_LOGGER;
}

namespace sihd::sys::internal
{

ExecEnviron to_exec_environ(const Environment & env)
{
    ExecEnviron ret;
    ret.entries.reserve(env.size());
    ret.array.reserve(env.size() + 1);
    for (const auto & [key, value] : env.entries())
    {
        // reserved up-front: the vector never reallocates, c_str stays valid
        ret.entries.emplace_back(fmt::format("{}={}", key, value));
        ret.array.emplace_back(ret.entries.back().c_str());
    }
    ret.array.emplace_back(nullptr);
    return ret;
}

void apply(const Environment & env)
{
    for (const auto & [key, value] : env.entries())
        SIHD_UNEXPECTED_LOG(env::set(key, value));
}

} // namespace sihd::sys::internal
