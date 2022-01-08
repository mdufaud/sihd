#include <sihd/ssh/SshShell.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LineReader.hpp>

namespace sihd::ssh
{

LOGGER;

SshShell::SshShell(ssh_session session): _ssh_session_ptr(session)
{
}

SshShell::~SshShell()
{
}

bool    SshShell::open(bool x11)
{
    ssh_channel channel_ptr = ssh_channel_new(_ssh_session_ptr);
    if (channel_ptr == nullptr)
    {
        LOG(error, "SshShell: failed to create a ssh channel: " << ssh_get_error(_ssh_session_ptr));
        return false;
    }
    _channel.set_channel(channel_ptr);
    bool ret = _channel.open_session();
    if (!ret)
        LOG(error, "SshShell: failed to open a ssh channel session");
    if (ret && _channel.request_pty() == false)
    {
        LOG(error, "SshShell: failed to request a ssh pty");
        ret = false;
    }
    if (ret && _channel.change_pty_size(80, 24) == false)
    {
        LOG(error, "SshShell: failed to change pty size");
        ret = false;
    }
    if (ret && x11 && _channel.request_x11("", "", 0, false) == false)
    {
        LOG(error, "SshShell: failed to request x11");
        ret = false;
    }
    if (ret && _channel.request_shell() == false)
    {
        LOG(error, "SshShell: failed to request shell");
        ret = false;
    }
    if (!ret)
        _channel.clear_channel();
    return ret;
}

bool    SshShell::read_loop()
{
    bool ret = true;
    sihd::util::LineReader reader("stdin-reader");
    reader.set_stream(stdin, false);
    reader.set_read_buffsize(1);
    reader.set_delimiter_in_line(true);
    size_t line_size;
    char *line;
    size_t bufsize = 4096;
    char buf[bufsize + 1];
    int nbytes;
    int nwritten;

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
            nbytes = _channel.read(buf, bufsize);
            if (nbytes < 0)
            {
                LOG(error, "SshShell: error reading channel");
                ret = false;
                break ;
            }
            if (nbytes > 0)
            {
                buf[nbytes] = 0;
                std::cout << buf;
                fflush(stdout);
            }
        }
        if (FD_ISSET(0, &fds))
        {
            if (reader.read_next())
            {
                reader.get_read_data(&line, &line_size);
                nwritten = _channel.write(line, line_size);
                if (nwritten != (int)line_size)
                {
                    LOG_ERROR("SshShell: error writing to channel '%d' != '%d'", nwritten, (int)line_size);
                    ret = false;
                    break ;
                }
            }
            else
            {
                if (reader.error())
                {
                    LOG(error, "SshShell: error reading stdin");
                    ret = false;
                }
                std::cout << std::endl;
                break ;
            }
        }
    }
    _channel.clear_channel();
    return ret;
}

void    SshShell::close()
{
    _channel.clear_channel();
}


}