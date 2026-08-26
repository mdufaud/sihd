#include <sihd/sys/platform.hpp>

#include <list>
#include <stdexcept>

#include <sihd/sys/FileWatcher.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>

// inotify backend. emscripten ships none -> FileWatcher is a no-op stub there.
#if !defined(__SIHD_EMSCRIPTEN__)
# define FILEWATCHER_BACKEND
# define INOTIFY_ENABLED

# define EVENT_SIZE (sizeof(struct inotify_event))
# define EVENT_BUFFER_LEN (5 * (EVENT_SIZE + NAME_MAX))

# include <sys/inotify.h>
# include <sys/ioctl.h>
# include <unistd.h>

#endif

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

struct FileWatcher::Impl: public sihd::util::IHandler<sihd::sys::Poll *>
{
        struct Watcher
        {
                Watcher() = default;
                ~Watcher() { this->close(); }

                Watcher(Watcher && other) noexcept { *this = std::move(other); }

                Watcher & operator=(Watcher && other) noexcept
                {
                    if (this != &other)
                    {
                        this->close();
                        path = std::move(other.path);
                        inotify_fd = other.inotify_fd;
                        watch_fd = other.watch_fd;
                        old_filename = std::move(other.old_filename);
                        other.watch_fd = -1;
                    }
                    return *this;
                }

                void close()
                {
#if defined(INOTIFY_ENABLED)
                    if (watch_fd >= 0 && inotify_fd >= 0)
                    {
                        inotify_rm_watch(inotify_fd, watch_fd);
                        watch_fd = -1;
                        inotify_fd = -1;
                    }
#endif
                }

                std::string path;
                int inotify_fd = -1;
                int watch_fd = -1;
                std::string old_filename;
        };

        Impl(std::vector<FileWatcherEvent> & events): _events(events)
        {
#if defined(FILEWATCHER_BACKEND)
            _buffer.resize(EVENT_BUFFER_LEN);
#endif
        }
        ~Impl() { this->terminate(); }

        std::string _buffer;
        std::vector<FileWatcherEvent> & _events;

        int _inotify_fd = -1;
        Poll _poll;
        std::list<Watcher> _watchers;

        void handle(Poll *poll);

        void init();
        bool add_watch(std::string_view path);
        bool rm_watch(std::string_view path);
        bool is_watching(std::string_view path);
        void terminate();
        bool poll_new_events(int milliseconds_timeout);
};

bool FileWatcher::Impl::is_watching(std::string_view path)
{
    return std::find_if(_watchers.begin(),
                        _watchers.end(),
                        [path](const Watcher & w) { return w.path == path; })
           != _watchers.end();
}

bool FileWatcher::Impl::rm_watch(std::string_view path)
{
    auto it = std::find_if(_watchers.begin(), _watchers.end(), [path](const Watcher & w) {
        return w.path == path;
    });
    const bool found = it != _watchers.end();
    if (found)
    {
        it->close();
        _watchers.erase(it);
    }
    return found;
}

void FileWatcher::Impl::init()
{
#if defined(INOTIFY_ENABLED)
    _inotify_fd = inotify_init();
    if (_inotify_fd < 0)
    {
        throw std::runtime_error(os::last_error_str());
    }
    _poll.set_limit(1);
    _poll.set_read_fd(_inotify_fd);
    _poll.add_observer(this);
#else
    (void)this;
#endif
}

bool FileWatcher::Impl::add_watch(std::string_view path)
{
#if defined(INOTIFY_ENABLED)
    if (this->is_watching(path))
        return true;

    int watch_fd = inotify_add_watch(_inotify_fd, path.data(), IN_ALL_EVENTS);
    if (watch_fd < 0)
    {
        SIHD_LOG(error, "FileWatcher: {}", os::last_error_str());
        return false;
    }
    Watcher watcher;
    watcher.path = std::string(path);
    watcher.inotify_fd = _inotify_fd;
    watcher.watch_fd = watch_fd;
    _watchers.emplace_back(std::move(watcher));

    return true;
#else
    (void)path;
    return false;
#endif
}

bool FileWatcher::Impl::poll_new_events(int milliseconds_timeout)
{
#if defined(INOTIFY_ENABLED)
    return _inotify_fd >= 0 && _poll.poll(milliseconds_timeout) > 0;
#else
    (void)milliseconds_timeout;
    return false;
#endif
}

void FileWatcher::Impl::terminate()
{
#if defined(INOTIFY_ENABLED)
    _watchers.clear();
    if (_inotify_fd >= 0)
    {
        _poll.clear_fd(_inotify_fd);
        close(_inotify_fd);
        _inotify_fd = -1;
    }
#endif
}

#if defined(INOTIFY_ENABLED)
void FileWatcher::Impl::handle(Poll *poll)
{
    for (const auto & event : poll->events())
    {
        if (event.readable)
        {
            // inotify specifies that you can read the number of bytes available in the buffer
            int avail;
            if (ioctl(_inotify_fd, FIONREAD, &avail) < 0)
            {
                SIHD_LOG(error, "FileWatcher: ioctl error {}", os::last_error_str());
                return;
            }
            if (avail > (int)_buffer.size())
            {
                _buffer.resize(avail);
            }

            // pray that we don't have a truncated event if avail > EVENT_BUFFER_LEN
            ssize_t length = read(_inotify_fd, _buffer.data(), avail);
            if (length < 0)
            {
                SIHD_LOG(error, "FileWatcher: read error {}", os::last_error_str());
                return;
            }
            else if ((int)length != avail)
            {
                SIHD_LOG(error, "FileWatcher: read event size error {} != {}", length, avail);
                return;
            }

            int offset = 0;
            while (offset < length)
            {
                struct inotify_event *event = (struct inotify_event *)&_buffer.data()[offset];

                auto it = std::find_if(_watchers.begin(), _watchers.end(), [event](const Watcher & w) {
                    return w.watch_fd == event->wd;
                });

                if (it == _watchers.end())
                {
                    SIHD_LOG_WARN("FileWatcher: watch not found {}", event->wd);
                    offset += EVENT_SIZE + event->len;
                    continue;
                }

                bool add_event = true;
                Watcher & watcher = *it;

                FileWatcherEvent fw_event;
                fw_event.watch_path = watcher.path;
                if (event->len > 0)
                    fw_event.filename = std::string(event->name);

                if (event->mask & IN_CREATE)
                {
                    fw_event.type = FileWatcherEventType::created;
                }
                else if (event->mask & IN_DELETE)
                {
                    fw_event.type = FileWatcherEventType::deleted;
                }
                else if (event->mask & IN_MODIFY)
                {
                    fw_event.type = FileWatcherEventType::modified;
                }
                else if (event->mask & IN_OPEN)
                {
                    fw_event.type = FileWatcherEventType::opened;
                }
                else if (event->mask & IN_ACCESS)
                {
                    fw_event.type = FileWatcherEventType::accessed;
                }
                else if (event->mask & IN_CLOSE)
                {
                    fw_event.type = FileWatcherEventType::closed;
                }
                else if (event->mask & IN_MOVED_FROM)
                {
                    watcher.old_filename = std::move(fw_event.filename);
                    add_event = false;
                }
                else if (event->mask & IN_MOVED_TO)
                {
                    fw_event.type = FileWatcherEventType::renamed;
                    fw_event.old_filename = std::move(watcher.old_filename);
                    watcher.old_filename.clear();
                }

                else if (event->mask & IN_DELETE_SELF || event->mask & IN_MOVE_SELF
                         || event->mask & IN_UNMOUNT || event->mask & IN_Q_OVERFLOW
                         || event->mask & IN_IGNORED)
                {
                    // not supported or termination events
                    fw_event.type = FileWatcherEventType::terminated;
                    this->rm_watch(watcher.path);
                }
                else
                {
                    add_event = false;
                }

                if (add_event)
                {
                    _events.emplace_back(std::move(fw_event));
                }

                offset += EVENT_SIZE + event->len;
            }
        }
        if (event.closed || event.error)
        {
            for (auto & watcher : _watchers)
            {
                _events.emplace_back(
                    FileWatcherEvent {.type = FileWatcherEventType::terminated, .watch_path = watcher.path});
            }
            this->terminate();
        }
    }
}
#endif

FileWatcher::FileWatcher()
{
    _impl = std::make_unique<FileWatcher::Impl>(_events);
    _impl->init();
    _run_timeout_milliseconds = 100;
};

FileWatcher::FileWatcher(std::string_view path): FileWatcher()
{
    if (!_impl->add_watch(path))
    {
        throw std::runtime_error(os::last_error_str());
    }
}

FileWatcher::FileWatcher(std::string_view path, int run_timeout_milliseconds): FileWatcher(path)
{
    this->set_run_timeout(run_timeout_milliseconds);
}

FileWatcher::~FileWatcher() = default;

bool FileWatcher::run()
{
    _events.clear();
    const bool success = _impl->poll_new_events(_run_timeout_milliseconds);
    if (success)
        this->notify_observers(this);
    return success;
}

bool FileWatcher::watch(std::string_view path)
{
    return _impl->add_watch(path);
}

bool FileWatcher::unwatch(std::string_view path)
{
    return _impl->rm_watch(path);
}

bool FileWatcher::is_watching(std::string_view path) const
{
    return _impl->is_watching(path);
}

void FileWatcher::clear()
{
    _impl->terminate();
}

} // namespace sihd::sys
