#ifndef __SIHD_SYS_LINUX_INTERNAL_ENVIRONMENT_HPP__
#define __SIHD_SYS_LINUX_INTERNAL_ENVIRONMENT_HPP__

#include <vector>

#include <sihd/sys/Environment.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::internal
{

// null-terminated "KEY=VALUE" array pointing into entries: entries must outlive it
struct ExecEnviron
{
        std::vector<std::string> entries;
        std::vector<const char *> array;
};

ExecEnviron to_exec_environ(const Environment & env);
sihd::util::Result<void> apply(const Environment & env);

} // namespace sihd::sys::internal

#endif
