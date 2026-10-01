#include <mutex>

#include <libssh/libssh.h>

#include <sihd/ssh/utils.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fmt.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::ssh::utils
{

SIHD_NEW_LOGGER("sihd::ssh::utils");

namespace
{
std::mutex _init_mutex;
int _ref_count = 0;
} // namespace

std::expected<void, Error> init()
{
    std::lock_guard l(_init_mutex);
    if (_ref_count == 0)
    {
        const int ret = ssh_init();
        if (ret != SSH_OK)
            return std::unexpected(Error(unknown, "ssh_init() failed with code {}", ret));
    }
    ++_ref_count;
    return {};
}

std::expected<void, Error> finalize()
{
    std::lock_guard l(_init_mutex);
    if (_ref_count == 0)
        return std::unexpected(Error(not_initialized, "ssh not initialized"));
    --_ref_count;
    if (_ref_count == 0)
    {
        const int ret = ssh_finalize();
        if (ret != SSH_OK)
            return std::unexpected(Error(unknown, "ssh_finalize() failed with code {}", ret));
    }
    return {};
}

bool is_initialized()
{
    std::lock_guard l(_init_mutex);
    return _ref_count > 0;
}

} // namespace sihd::ssh::utils
