#include <libssh/libssh.h>

#include <sihd/sys/LineReader.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fmt.hpp>

#if defined(__SIHD_WINDOWS__)
# include <conio.h>
#endif

#include <sihd/ssh/SshShell.hpp>
#include <sihd/ssh/utils.hpp>

#include "ssh_error.hpp"

namespace sihd::ssh
{

using namespace sihd::util;
using namespace sihd::sys;

SIHD_LOGGER;

struct SshShell::Impl
{
        ssh_session_struct *ssh_session_ptr;
        SshChannel channel;
};

SshShell::SshShell(void *session): _impl_ptr(std::make_unique<Impl>())
{
    _impl_ptr->ssh_session_ptr = static_cast<ssh_session_struct *>(session);
    SIHD_UNEXPECTED_LOG(utils::init());
}

SshShell::~SshShell()
{
    SIHD_UNEXPECTED_LOG(utils::finalize());
}

SshChannel & SshShell::channel()
{
    return _impl_ptr->channel;
}

std::expected<void, Error> SshShell::open(bool x11)
{
    ssh_channel channel_ptr = ssh_channel_new(_impl_ptr->ssh_session_ptr);
    if (channel_ptr == nullptr)
        return std::unexpected(Error(sihd::util::ErrorCode::not_initialized,
                                     "could not create a ssh channel: {}",
                                     ssh_error_str(_impl_ptr->ssh_session_ptr)));
    _impl_ptr->channel.set_channel(channel_ptr);

    long columns = 80;
    if (const std::optional<std::string> env_columns = env::get("COLUMNS"))
    {
        if (const auto val = str::convert_from_string<long>(*env_columns); val.has_value() && *val > 0)
            columns = *val;
    }
    long rows = 24;
    if (const std::optional<std::string> env_rows = env::get("LINES"))
    {
        if (const auto val = str::convert_from_string<long>(*env_rows); val.has_value() && *val > 0)
            rows = *val;
    }

    std::expected<void, Error> res = _impl_ptr->channel.open_session();
    if (res)
        res = _impl_ptr->channel.request_pty();
    if (res)
        res = _impl_ptr->channel.change_pty_size(columns, rows);
    if (res && x11)
        res = _impl_ptr->channel.request_x11("", "", 0, false);
    if (res)
        res = _impl_ptr->channel.request_shell();
    if (!res)
    {
        _impl_ptr->channel.clear_channel();
        return res;
    }
    return {};
}

bool SshShell::read_loop()
{
    int nbytes;
    int nwritten;
    bool ret = true;

    sihd::util::ArrCharView view;
    sihd::util::ArrChar buf;
    if (!buf.reserve(4096))
        return false;

    sihd::sys::LineReader reader({
        .read_buffsize = 1,
        .delimiter_in_line = true,
    });
    (void)reader.set_stream(stdin);

    while (_impl_ptr->channel.is_open() && _impl_ptr->channel.is_eof() == false)
    {
        struct timeval timeout;
        ssh_channel in_channels[2];
        ssh_channel out_channels[2];
        fd_set fds;
        int maxfd;

        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        in_channels[0] = static_cast<ssh_channel>(_impl_ptr->channel.channel());
        in_channels[1] = nullptr;
        FD_ZERO(&fds);
#if !defined(__SIHD_WINDOWS__)
        FD_SET(0, &fds);
        FD_SET(ssh_get_fd(_impl_ptr->ssh_session_ptr), &fds);
        maxfd = ssh_get_fd(_impl_ptr->ssh_session_ptr) + 1;
#else
        FD_SET(ssh_get_fd(_impl_ptr->ssh_session_ptr), &fds);
        maxfd = ssh_get_fd(_impl_ptr->ssh_session_ptr) + 1;
#endif

        ssh_select(in_channels, out_channels, maxfd, &fds, &timeout);

        if (out_channels[0] != nullptr)
        {
            if (_impl_ptr->channel.poll())
            {
                nbytes = _impl_ptr->channel.read(buf);
                if (nbytes < 0)
                {
                    // might be just user typed $> exit
                    break;
                }
                if (nbytes > 0)
                {
                    fmt::print(stdout, "{}", buf);
                    fflush(stdout);
                }
            }
            if (_impl_ptr->channel.poll_stderr())
            {
                nbytes = _impl_ptr->channel.read_stderr(buf);
                if (nbytes < 0)
                {
                    // might be just user typed $> exit
                    break;
                }
                if (nbytes > 0)
                {
                    fmt::print(stderr, "{}", buf);
                    fflush(stderr);
                }
            }
        }
        // Check for stdin input
#if !defined(__SIHD_WINDOWS__)
        bool stdin_ready = FD_ISSET(0, &fds);
#else
        bool stdin_ready = _kbhit();
#endif
        if (stdin_ready)
        {
            auto line = reader.read_next();
            if (line && *line)
            {
                reader.get_read_data(view);
                nwritten = _impl_ptr->channel.write(view);
                if (nwritten != (int)view.size())
                {
                    SIHD_LOG(error, "error writing to channel '{}' != '{}'", nwritten, view.size());
                    ret = false;
                    break;
                }
            }
            else
            {
                if (!line)
                {
                    SIHD_LOG(error, "error reading stdin: {}", line.error().message);
                    ret = false;
                }
                fmt::print("\n");
                break;
            }
        }
    }
    if (!ret)
        SIHD_UNEXPECTED_LOG(_impl_ptr->channel.send_eof());
    _impl_ptr->channel.clear_channel();
    return ret;
}

void SshShell::close()
{
    _impl_ptr->channel.clear_channel();
}

} // namespace sihd::ssh
