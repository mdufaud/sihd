#include <sihd/util/platform.hpp>

#include <sys/inotify.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

#include <sihd/util/FileWatcher.hpp>
#include <sihd/util/Logger.hpp>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (10 * (EVENT_SIZE + NAME_MAX))

namespace sihd::util
{

SIHD_LOGGER;

FileWatcher::FileWatcher(std::string_view path, Callback && callback): _fd(-1), _callback(std::move(callback))
{
    _fd = inotify_init();
    if (_fd < 0)
    {
        throw std::runtime_error(strerror(errno));
    }

    _wd = inotify_add_watch(_fd, path.data(), IN_ALL_EVENTS);
    if (_wd < 0)
    {
        throw std::runtime_error(strerror(errno));
    }
    _path = path;

    _poll.set_limit(1);
    _poll.set_read_fd(_fd);
    _poll.add_observer(this);
}

FileWatcher::~FileWatcher()
{
    this->rm_watch();
}

bool FileWatcher::poll()
{
    return _poll.poll(100) > 0;
}

void FileWatcher::rm_watch()
{
    if (_wd >= 0)
    {
        inotify_rm_watch(_fd, _wd);
        _wd = -1;
    }
    if (_fd >= 0)
    {
        close(_fd);
        _fd = -1;
    }
}

void FileWatcher::handle(Poll *poll)
{
    for (const auto event : poll->events())
    {
        if (event.readable)
        {
            char buffer[EVENT_BUF_LEN];

            int length = read(_fd, buffer, EVENT_BUF_LEN);
            if (length < 0)
            {
                SIHD_LOG(error, "FileWatcher: read error {}", strerror(errno));
                return;
            }
            int i = 0;
            while (i < length)
            {
                struct inotify_event *event = (struct inotify_event *)&buffer[i];
                if (event->wd == _wd)
                {
                    SIHD_TRACEV(event->cookie);
                    SIHD_TRACEV(event->mask);
                    SIHD_TRACEV(event->len);
                    std::string_view event_name;
                    if (event->len > 0)
                        event_name = std::string_view(event->name, event->len);
                    else
                        event_name = _path;
                    if (event->mask & IN_CREATE)
                    {
                        SIHD_LOG(info, "Subfile created: {}", event_name);
                    }
                    if (event->mask & IN_DELETE)
                    {
                        SIHD_LOG(info, "Subfile deleted: {}", event_name);
                    }
                    if (event->mask & IN_DELETE_SELF)
                    {
                        SIHD_LOG(info, "Self deleted: {}", event_name);
                    }
                    if (event->mask & IN_MOVE_SELF)
                    {
                        SIHD_LOG(info, "Self moved: {}", event_name);
                    }
                    if (event->mask & IN_MODIFY)
                    {
                        SIHD_LOG(info, "Modified: {}", event_name);
                    }
                    if (event->mask & IN_OPEN)
                    {
                        SIHD_LOG(info, "Open: {}", event_name);
                    }
                    if (event->mask & IN_ACCESS)
                    {
                        SIHD_LOG(info, "Accessed: {}", event_name);
                    }
                    if (event->mask & IN_MOVED_FROM)
                    {
                        SIHD_LOG(info, "Moved from: {}", event_name);
                    }
                    if (event->mask & IN_MOVED_TO)
                    {
                        SIHD_LOG(info, "Moved to: {}", event_name);
                    }
                    if (event->mask & IN_CLOSE)
                    {
                        SIHD_LOG(info, "Close: {}", event_name);
                    }
                }
                i += EVENT_SIZE + event->len;
            }
        }
        if (event.closed)
        {
            this->rm_watch();
        }
        if (event.error)
        {
            this->rm_watch();
        }
    }
}

} // namespace sihd::util
