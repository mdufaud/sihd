#include <mutex>

#include <libssh/libssh.h>

#include <sihd/util/Logger.hpp>

#include <sihd/ssh/utils.hpp>

namespace sihd::ssh::utils
{

SIHD_NEW_LOGGER("sihd::ssh::utils");

namespace
{
std::mutex _init_mutex;
int _ref_count = 0;
} // namespace

bool init()
{
    std::lock_guard l(_init_mutex);
    if (_ref_count == 0)
    {
        int ret = ssh_init();
        if (ret != SSH_OK)
        {
            SIHD_LOG(error, "ssh_init() failed with code {}", ret);
            return false;
        }
    }
    ++_ref_count;
    return true;
}

bool finalize()
{
    std::lock_guard l(_init_mutex);
    if (_ref_count == 0)
    {
        return false;
    }
    --_ref_count;
    if (_ref_count == 0)
    {
        int ret = ssh_finalize();
        if (ret != SSH_OK)
        {
            SIHD_LOG(error, "ssh_finalize() failed with code {}", ret);
            return false;
        }
    }
    return true;
}

bool is_initialized()
{
    std::lock_guard l(_init_mutex);
    return _ref_count > 0;
}

} // namespace sihd::ssh::utils
