#include <libssh/libssh.h>

#include <sihd/ssh/SshScp.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>

#ifndef SIHD_SSH_SCP_BUFFSIZE
# define SIHD_SSH_SCP_BUFFSIZE 4096
#endif

namespace sihd::ssh
{

SIHD_LOGGER;

SshScp::SshScp(ssh_session session): _remote_opened(false), _ssh_scp_ptr(nullptr), _ssh_session_ptr(session) {}

SshScp::~SshScp()
{
    this->close();
}

void SshScp::close()
{
    if (_ssh_scp_ptr != nullptr)
    {
        ssh_scp_close(_ssh_scp_ptr);
        ssh_scp_free(_ssh_scp_ptr);
        _ssh_scp_ptr = nullptr;
        _remote_opened = false;
    }
}

bool SshScp::_open(int flags, std::string_view location)
{
    this->close();
    _ssh_scp_ptr = ssh_scp_new(_ssh_session_ptr, flags, location.data());
    if (_ssh_scp_ptr == nullptr)
    {
        SIHD_LOG(error, "SshScp: failed to create new scp session: {}", ssh_get_error(_ssh_session_ptr));
        return false;
    }
    if (ssh_scp_init(_ssh_scp_ptr) != SSH_OK)
    {
        SIHD_LOG(error, "SshScp: failed to initialize scp session: {}", ssh_get_error(_ssh_session_ptr));
        this->close();
        return false;
    }
    return true;
}

bool SshScp::remote_opened()
{
    return _remote_opened;
}

bool SshScp::open_remote(std::string_view remote_path)
{
    _remote_opened = this->_open(SSH_SCP_WRITE | SSH_SCP_RECURSIVE, remote_path);
    return _remote_opened;
}

bool SshScp::leave_dir()
{
    if (_remote_opened)
    {
        if (ssh_scp_leave_directory(_ssh_scp_ptr) == SSH_OK)
            return true;
        SIHD_LOG(error, "SshScp: failed leaving directory: {}", ssh_get_error(_ssh_session_ptr));
    }
    else
        SIHD_LOG(error, "SshScp: cannot leave directory, no remote is opened");
    return false;
}

bool SshScp::push_dir(std::string_view name, int mode)
{
    if (_remote_opened == false)
        return false;

    int r = ssh_scp_push_directory(_ssh_scp_ptr, name.data(), mode);
    if (r != SSH_OK)
        SIHD_LOG(error, "SshScp: failed to create directory: {}", ssh_get_error(_ssh_session_ptr));
    return r == SSH_OK;
}

bool SshScp::push_file(std::string_view local_path, std::string_view remote_path, size_t max_size, int mode)
{
    if (_remote_opened == false)
        return false;

    sihd::util::File file(local_path, "rb");
    if (file.is_open() == false)
        return false;
    char buf[SIHD_SSH_SCP_BUFFSIZE + 1];
    size_t size = sihd::util::fs::file_size(local_path);
    if (max_size > 0)
        size = std::max(size, max_size);

    ssize_t read_ret;
    int r = ssh_scp_push_file(_ssh_scp_ptr, remote_path.data(), size, mode);
    if (r != SSH_OK)
        SIHD_LOG(error, "SshScp: failed to initialize file push: {}", ssh_get_error(_ssh_session_ptr));
    while (r == SSH_OK && !file.error() && !file.eof())
    {
        read_ret = file.read(buf, SIHD_SSH_SCP_BUFFSIZE);
        if (read_ret > 0)
        {
            r = ssh_scp_write(_ssh_scp_ptr, buf, read_ret);
            if (r != SSH_OK)
                SIHD_LOG(error, "SshScp: failed to write while pushing file: {}", ssh_get_error(_ssh_session_ptr));
        }
    }
    return r == SSH_OK;
}

bool SshScp::push_file_content(std::string_view remote_path, std::string_view data, int mode)
{
    if (_remote_opened == false)
        return false;
    int r = ssh_scp_push_file(_ssh_scp_ptr, remote_path.data(), data.size(), mode);
    if (r != SSH_OK)
    {
        SIHD_LOG(error, "SshScp: failed to initialize file push: {}", ssh_get_error(_ssh_session_ptr));
        return false;
    }
    return ssh_scp_write(_ssh_scp_ptr, data.data(), data.size());
}

bool SshScp::pull_file(std::string_view remote_path, std::string_view local_path)
{
    sihd::util::File file(local_path, "wb");
    if (file.is_open() == false)
        return false;

    if (this->_open(SSH_SCP_READ, remote_path.data()) == false)
        return false;

    int r = ssh_scp_pull_request(_ssh_scp_ptr);
    if (r != SSH_SCP_REQUEST_NEWFILE)
    {
        SIHD_LOG(error, "SshScp: failed to retrieve pull informations: {}", ssh_get_error(_ssh_session_ptr));
        this->close();
        return false;
    }

    size_t total = ssh_scp_request_get_size(_ssh_scp_ptr);
    if (ssh_scp_accept_request(_ssh_scp_ptr) != SSH_OK)
    {
        SIHD_LOG(error, "SshScp: failed to accept pull request: {}", ssh_get_error(_ssh_session_ptr));
        this->close();
        return false;
    }

    char buf[SIHD_SSH_SCP_BUFFSIZE + 1];
    size_t wrote = 0;
    while (wrote < total)
    {
        r = ssh_scp_read(_ssh_scp_ptr, buf, SIHD_SSH_SCP_BUFFSIZE);
        if (r == SSH_ERROR)
        {
            SIHD_LOG(error, "SshScp: failed to read: {}", ssh_get_error(_ssh_session_ptr));
            break;
        }
        if (file.write(buf, r) < 0)
        {
            SIHD_LOG(error, "SshScp: failed writing file: {}", local_path);
            break;
        }
        wrote += r;
        r = ssh_scp_pull_request(_ssh_scp_ptr);
        if (r == SSH_SCP_REQUEST_EOF)
            break;
    }
    return wrote == total;
}

const char *SshScp::get_warning()
{
    return _ssh_scp_ptr != nullptr ? ssh_scp_request_get_warning(_ssh_scp_ptr) : nullptr;
}

} // namespace sihd::ssh
