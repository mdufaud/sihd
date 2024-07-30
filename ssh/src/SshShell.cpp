#include <libssh/libssh.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/LineReader.hpp>
#include <sihd/util/Logger.hpp>

#include <sihd/ssh/SshShell.hpp>

namespace sihd::ssh
{

using namespace sihd::util;

SIHD_LOGGER;

SshShell::SshShell(ssh_session_struct *session): _ssh_session_ptr(session) {}

SshShell::~SshShell() {}

bool SshShell::open(bool x11)
{
    ssh_channel channel_ptr = ssh_channel_new(_ssh_session_ptr);
    if (channel_ptr == nullptr)
    {
        SIHD_LOG(error, "SshShell: failed to create a ssh channel: {}", ssh_get_error(_ssh_session_ptr));
        return false;
    }
    _channel.set_channel(channel_ptr);
    bool ret = _channel.open_session();
    if (!ret)
        SIHD_LOG(error, "SshShell: failed to open a ssh channel session");
    if (ret && _channel.request_pty() == false)
    {
        SIHD_LOG(error, "SshShell: failed to request a ssh pty");
        ret = false;
    }

    long columns;
    const char *env_columns = getenv("COLUMNS");
    if (env_columns == nullptr || str::to_long(env_columns, &columns, 10) == false || columns <= 0)
        columns = 80;
    long rows;
    const char *env_rows = getenv("LINES");
    if (env_rows == nullptr || str::to_long(env_rows, &rows, 10) == false || rows <= 0)
        rows = 24;

    if (ret && _channel.change_pty_size(columns, rows) == false)
    {
        SIHD_LOG(error, "SshShell: failed to change pty size");
        ret = false;
    }
    if (ret && x11 && _channel.request_x11("", "", 0, false) == false)
    {
        SIHD_LOG(error, "SshShell: failed to request x11");
        ret = false;
    }
    if (ret && _channel.request_shell() == false)
    {
        SIHD_LOG(error, "SshShell: failed to request shell");
        ret = false;
    }
    if (!ret)
        _channel.clear_channel();
    return ret;
}

bool SshShell::read_loop()
{
    int nbytes;
    int nwritten;
    bool ret = true;
    sihd::util::ArrCharView view;
    sihd::util::ArrChar buf;
    buf.reserve(4096);
    sihd::util::LineReader reader({
        .read_buffsize = 1,
        .delimiter_in_line = true,
    });
    reader.set_stream(stdin, false);

    while (_channel.is_open() && _channel.is_eof() == false)
    {
        struct timeval timeout;
        ssh_channel in_channels[2];
        ssh_channel out_channels[2];
        fd_set fds;
        int maxfd;

        timeout.tv_sec = 30;
        timeout.tv_usec = 0;
        in_channels[0] = _channel.channel();
        in_channels[1] = nullptr;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(ssh_get_fd(_ssh_session_ptr), &fds);
        maxfd = ssh_get_fd(_ssh_session_ptr) + 1;

        ssh_select(in_channels, out_channels, maxfd, &fds, &timeout);

        if (out_channels[0] != nullptr)
        {
            nbytes = _channel.read(buf);
            if (nbytes < 0)
            {
                // might be just user typed $> exit
                // SIHD_LOG(error, "SshShell: error reading channel");
                // ret = false;
                break;
            }
            if (nbytes > 0)
            {
                std::string_view view = buf;
                fmt::print("{}", view);
                fflush(stdout);
            }
        }
        if (FD_ISSET(0, &fds))
        {
            if (reader.read_next())
            {
                reader.get_read_data(view);
                nwritten = _channel.write(view);
                if (nwritten != (int)view.size())
                {
                    SIHD_LOG_ERROR("SshShell: error writing to channel '{}' != '{}'", nwritten, view.size());
                    ret = false;
                    break;
                }
            }
            else
            {
                if (reader.error())
                {
                    SIHD_LOG(error, "SshShell: error reading stdin");
                    ret = false;
                }
                fmt::print("\n");
                break;
            }
        }
    }
    _channel.clear_channel();
    return ret;
}

void SshShell::close()
{
    _channel.clear_channel();
}

} // namespace sihd::ssh
