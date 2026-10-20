#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <string>

# include <sihd/sys/linux/ns.hpp>
# include <sihd/sys/os.hpp>

# if !defined(__SIHD_EMSCRIPTEN__)
#  include <fcntl.h>
#  include <sched.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  ifndef CLONE_NEWTIME
#   define CLONE_NEWTIME 0x00000080
#  endif
# endif

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::linux_::ns
{

std::string_view to_str(Type type)
{
    switch (type)
    {
        case Type::cgroup:
            return "cgroup";
        case Type::ipc:
            return "ipc";
        case Type::mount:
            return "mnt";
        case Type::net:
            return "net";
        case Type::pid:
            return "pid";
        case Type::pid_for_children:
            return "pid_for_children";
        case Type::time:
            return "time";
        case Type::time_for_children:
            return "time_for_children";
        case Type::user:
            return "user";
        case Type::uts:
            return "uts";
    }
    return "";
}

Result<uint64_t> id(Type type)
{
    return id(type, os::pid());
}

# if !defined(__SIHD_EMSCRIPTEN__)

namespace
{

int clone_flag(Type type)
{
    switch (type)
    {
        case Type::cgroup:
            return CLONE_NEWCGROUP;
        case Type::ipc:
            return CLONE_NEWIPC;
        case Type::mount:
            return CLONE_NEWNS;
        case Type::net:
            return CLONE_NEWNET;
        case Type::pid:
        case Type::pid_for_children:
            return CLONE_NEWPID;
        case Type::time:
        case Type::time_for_children:
            return CLONE_NEWTIME;
        case Type::user:
            return CLONE_NEWUSER;
        case Type::uts:
            return CLONE_NEWUTS;
    }
    return 0;
}

} // namespace

Result<uint64_t> id(Type type, pid_t pid)
{
    const std::string path = "/proc/" + std::to_string(pid) + "/ns/" + std::string(to_str(type));
    struct stat st = {};
    if (::stat(path.c_str(), &st) == -1)
        return std::unexpected(Error::from_errno("could not stat '{}'", path));
    return static_cast<uint64_t>(st.st_ino);
}

Result<void> unshare(Type type)
{
    if (::unshare(clone_flag(type)) == -1)
        return std::unexpected(Error::from_errno("could not unshare {} namespace", to_str(type)));
    return {};
}

Result<void> unshare(std::span<const Type> types)
{
    int flags = 0;
    for (const Type type : types)
        flags |= clone_flag(type);
    if (::unshare(flags) == -1)
        return std::unexpected(Error::from_errno("could not unshare namespaces"));
    return {};
}

Result<void> setns(std::string_view path, Type type)
{
    const std::string ns_path(path);
    const int fd = ::open(ns_path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd == -1)
        return std::unexpected(Error::from_errno("could not open '{}'", path));
    auto joined = setns(fd, type);
    ::close(fd);
    return joined;
}

Result<void> setns(int fd, Type type)
{
    if (::setns(fd, clone_flag(type)) == -1)
        return std::unexpected(Error::from_errno("could not setns {} namespace", to_str(type)));
    return {};
}

# else

Result<uint64_t> id(Type, pid_t)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<void> unshare(Type)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<void> unshare(std::span<const Type>)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<void> setns(std::string_view, Type)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<void> setns(int, Type)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

# endif

} // namespace sihd::sys::linux_::ns

#endif
