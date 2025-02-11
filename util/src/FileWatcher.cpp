#include <sihd/util/platform.hpp>

#include <cstring>
#include <stdexcept>

#include <sihd/util/FileWatcher.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/fs.hpp>

#if defined(__SIHD_WINDOWS__)
# include <windows.h>

# define EVENT_SIZE (sizeof(FILE_NOTIFY_INFORMATION))
# define EVENT_BUFFER_LEN (20 * (EVENT_SIZE + MAX_PATH))

#else

# if !defined(__SIHD_EMSCRIPTEN__)
#  define INOTIFY_ENABLED
# endif

# if defined(INOTIFY_ENABLED)
#  define EVENT_SIZE (sizeof(struct inotify_event))
#  define EVENT_BUFFER_LEN (5 * (EVENT_SIZE + NAME_MAX))

// no inotify with emscripten
#  include <sys/inotify.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
# endif

#endif

namespace sihd::util
{

SIHD_LOGGER;

std::string FileWatcherEvent::type_str() const
{
    switch (type)
    {
        case FileWatcherEventType::created:
            return "created";
        case FileWatcherEventType::deleted:
            return "deleted";
        case FileWatcherEventType::modified:
            return "modified";
        case FileWatcherEventType::renamed:
            return "renamed";
#if !defined(__SIHD_WINDOWS__)
        case FileWatcherEventType::opened:
            return "opened";
        case FileWatcherEventType::accessed:
            return "accessed";
        case FileWatcherEventType::closed:
            return "closed";
#endif
        default:
            return "unknown";
    }
}

#if defined(__SIHD_WINDOWS__) or !defined(INOTIFY_ENABLED)
struct FileWatcher::Impl
#else
struct FileWatcher::Impl: public sihd::util::IHandler<sihd::util::Poll *>
#endif
{
        struct Watcher
        {
                Watcher();
                ~Watcher() { this->close(); }

                Watcher(Watcher && other) noexcept { *this = std::move(other); }

                Watcher & operator=(Watcher && other) noexcept
                {
                    if (this != &other)
                    {
                        this->close();
                        path = std::move(other.path);
#if defined(__SIHD_WINDOWS__)
                        handle = other.handle;
                        other.handle = INVALID_HANDLE_VALUE;
                        memcpy(&overlapped, &other.overlapped, sizeof(overlapped));
                        memset(&other.overlapped, 0, sizeof(other.overlapped));
#else
# if defined(INOTIFY_ENABLED)
                        inotify_fd = other.inotify_fd;
                        watch_fd = other.watch_fd;
                        old_filename = std::move(other.old_filename);
                        other.watch_fd = -1;
# endif
#endif
                    }
                    return *this;
                }

                void close();

                std::string path;
#if defined(__SIHD_WINDOWS__)
                HANDLE handle = INVALID_HANDLE_VALUE;
                OVERLAPPED overlapped;
#else
# if defined(INOTIFY_ENABLED)
                int inotify_fd = -1;
                int watch_fd = -1;
                std::string old_filename;
# endif
#endif
        };

        Impl(std::vector<FileWatcherEvent> & events): _events(events)
        {
#if defined(INOTIFY_ENABLED)
            _buffer.resize(EVENT_BUFFER_LEN);
#endif
        }
        ~Impl() { this->terminate(); }

        std::string _buffer;
        std::vector<FileWatcherEvent> & _events;

#if !defined(__SIHD_WINDOWS__)
        int _inotify_fd = -1;
        Poll _poll;

# if defined(INOTIFY_ENABLED)
        void handle(Poll *poll);
# endif
#endif
        std::vector<Watcher> _watchers;

        void init();
        bool add_watch(std::string_view path);
        bool rm_watch(std::string_view path);
        bool is_watching(std::string_view path);
        void terminate();
        bool poll_new_events(int milliseconds_timeout);
};

bool FileWatcher::Impl::is_watching(std::string_view path)
{
    return std::find_if(_watchers.begin(), _watchers.end(), [path](const Watcher & w) { return w.path == path; })
           != _watchers.end();
}

bool FileWatcher::Impl::rm_watch(std::string_view path)
{
    auto it = std::find_if(_watchers.begin(), _watchers.end(), [path](const Watcher & w) { return w.path == path; });
    if (it != _watchers.end())
    {
        it->close();
        _watchers.erase(it);
    }
    return it != _watchers.end();
}

#if defined(__SIHD_WINDOWS__)

FileWatcher::Impl::Watcher::Watcher()
{
    memset(&overlapped, 0, sizeof(overlapped));
}

void FileWatcher::Impl::Watcher::close()
{
    if (handle == INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }

    if (overlapped.hEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(overlapped.hEvent);
        memset(&overlapped, 0, sizeof(overlapped));
    }
}

void FileWatcher::Impl::init() {}

bool FileWatcher::Impl::add_watch(std::string_view path)
{
    if (!fs::is_dir(path))
    {
        SIHD_LOG(error, "FileWatcher: Is not a directory: {}", path);
        return false;
    }
    if (this->is_watching(path))
        return true;

    Watcher watcher;
    watcher.handle = CreateFile(path.data(),
                                FILE_LIST_DIRECTORY,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                NULL);
    if (watcher.handle == INVALID_HANDLE_VALUE)
    {
        SIHD_LOG(error, "FileWatcher: {}", os::last_error_str());
        return false;
    }

    watcher.overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    constexpr bool watch_subtree = FALSE;
    if (!ReadDirectoryChangesW(watcher.handle,
                               _buffer.data(),
                               _buffer.size(),
                               watch_subtree,
                               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                                   | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE
                                   | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION
                                   | FILE_NOTIFY_CHANGE_SECURITY,
                               NULL,
                               &watcher.overlapped,
                               NULL))
    {
        SIHD_LOG(error, "FileWatcher: {}", os::last_error_str());
        return false;
    }

    watcher.path = std::string(path);
    _watchers.emplace_back(std::move(watcher));
    return true;
}

bool FileWatcher::Impl::poll_new_events(int milliseconds_timeout)
{
    std::list<std::string> watchers_to_remove;
    DWORD bytes_transferred;

    for (auto & watcher : _watchers)
    {
        DWORD result = WaitForSingleObject(watcher.overlapped.hEvent, milliseconds_timeout);
        if (result != WAIT_OBJECT_0)
        {
            continue;
        }

        GetOverlappedResult(watcher.handle, &watcher.overlapped, &bytes_transferred, FALSE);

        std::string old_filename;
        FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)_buffer.data();
        while (event != nullptr)
        {
            FileWatcherEvent fw_event;
            fw_event.watch_path = watcher.path;

            std::wstring ws(event->FileName, event->FileNameLength / sizeof(wchar_t));
            fw_event.filename = std::string(ws.begin(), ws.end());
            bool add_event = true;
            switch (event->Action)
            {
                case FILE_ACTION_ADDED:
                    fw_event.type = FileWatcherEventType::created;
                    break;
                case FILE_ACTION_REMOVED:
                    fw_event.type = FileWatcherEventType::deleted;
                    break;
                case FILE_ACTION_MODIFIED:
                    fw_event.type = FileWatcherEventType::modified;
                    break;
                case FILE_ACTION_RENAMED_OLD_NAME:
                    old_filename = fw_event.filename;
                    add_event = false;
                    break;
                case FILE_ACTION_RENAMED_NEW_NAME:
                    fw_event.type = FileWatcherEventType::renamed;
                    fw_event.old_filename = std::move(old_filename);
                    old_filename.clear();
                    break;
                default:
                    add_event = false;
                    break;
            }

            if (add_event)
            {
                _events.emplace_back(std::move(fw_event));
            }

            if (event->NextEntryOffset == 0)
            {
                break;
            }
            *((uint8_t **)&event) += event->NextEntryOffset;
        }

        constexpr bool watch_subtree = FALSE;
        if (!ReadDirectoryChangesW(watcher.handle,
                                   _buffer.data(),
                                   _buffer.size(),
                                   watch_subtree,
                                   FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                                       | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE
                                       | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION
                                       | FILE_NOTIFY_CHANGE_SECURITY,
                                   NULL,
                                   &watcher.overlapped,
                                   NULL))
        {
            _events.emplace_back(
                FileWatcherEvent {.type = FileWatcherEventType::terminated, .watch_path = watcher.path});
            watchers_to_remove.emplace_back(watcher.path);
            continue;
        }
    }

    for (auto & watcher_path : watchers_to_remove)
    {
        this->rm_watch(watcher_path);
    }

    return _events.size() > 0;
}

void FileWatcher::Impl::terminate()
{
    _watchers.clear();
}

#else // WINDOWS

FileWatcher::Impl::Watcher::Watcher() {}

void FileWatcher::Impl::Watcher::close()
{
# if defined(INOTIFY_ENABLED)
    if (watch_fd >= 0 && inotify_fd >= 0)
    {
        inotify_rm_watch(inotify_fd, watch_fd);
        watch_fd = -1;
        inotify_fd = -1;
    }
# endif
}

void FileWatcher::Impl::init()
{
# if defined(INOTIFY_ENABLED)
    _inotify_fd = inotify_init();
    if (_inotify_fd < 0)
    {
        throw std::runtime_error(strerror(errno));
    }
    _poll.set_limit(1);
    _poll.set_read_fd(_inotify_fd);
    _poll.add_observer(this);
# endif
}

bool FileWatcher::Impl::add_watch(std::string_view path)
{
# if defined(INOTIFY_ENABLED)
    if (this->is_watching(path))
        return true;

    int watch_fd = inotify_add_watch(_inotify_fd, path.data(), IN_ALL_EVENTS);
    if (watch_fd < 0)
    {
        SIHD_LOG(error, "FileWatcher: {}", strerror(errno));
        return false;
    }
    Watcher watcher;
    watcher.path = std::string(path);
    watcher.inotify_fd = _inotify_fd;
    watcher.watch_fd = watch_fd;
    _watchers.emplace_back(std::move(watcher));

    return true;
# else
    (void)path;
    return false;
# endif
}

bool FileWatcher::Impl::poll_new_events(int milliseconds_timeout)
{
# if defined(INOTIFY_ENABLED)
    return _inotify_fd >= 0 && _poll.poll(milliseconds_timeout) > 0;
# else
    (void)milliseconds_timeout;
    return false;
# endif
}

void FileWatcher::Impl::terminate()
{
# if defined(INOTIFY_ENABLED)
    _watchers.clear();
    if (_inotify_fd >= 0)
    {
        _poll.clear_fd(_inotify_fd);
        close(_inotify_fd);
        _inotify_fd = -1;
    }
# endif
}

# if defined(INOTIFY_ENABLED)
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
                SIHD_LOG(error, "FileWatcher: ioctl error {}", strerror(errno));
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
                SIHD_LOG(error, "FileWatcher: read error {}", strerror(errno));
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
                    return;
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

                else if (event->mask & IN_DELETE_SELF || event->mask & IN_MOVE_SELF || event->mask & IN_UNMOUNT
                         || event->mask & IN_Q_OVERFLOW || event->mask & IN_IGNORED)
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
# endif

#endif // LINUX

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

FileWatcher::~FileWatcher() {}

void FileWatcher::set_run_timeout(int milliseconds)
{
    _run_timeout_milliseconds = milliseconds;
}

const std::vector<FileWatcherEvent> & FileWatcher::events() const
{
    return _events;
}

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

} // namespace sihd::util
