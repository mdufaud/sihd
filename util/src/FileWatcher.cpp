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
# define EVENT_BUF_LEN (10 * (EVENT_SIZE + MAX_PATH))

#else

# define EVENT_SIZE (sizeof(struct inotify_event))
# define EVENT_BUF_LEN (10 * (EVENT_SIZE + NAME_MAX))

# include <sys/inotify.h>
# include <unistd.h>

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

#if defined(__SIHD_WINDOWS__)
struct FileWatcher::Impl
#else
struct FileWatcher::Impl: public sihd::util::IHandler<sihd::util::Poll *>
#endif
{
        Impl(std::vector<FileWatcherEvent> & events): _events(events) {}
        ~Impl() { this->terminate(); }

        std::string _watch_path;
        char _buffer[EVENT_BUF_LEN];
        std::vector<FileWatcherEvent> & _events;

#if defined(__SIHD_WINDOWS__)
        HANDLE _dir = INVALID_HANDLE_VALUE;
        OVERLAPPED _overlapped;
#else
        int _fd = -1;
        int _wd = -1;
        Poll _poll;

        void handle(Poll *poll);
#endif
        void init();
        void add_watch(std::string_view path);
        void terminate();
        bool poll_new_events(int milliseconds_timeout);
};

#if defined(__SIHD_WINDOWS__)

void FileWatcher::Impl::init()
{
    memset(&_overlapped, 0, sizeof(_overlapped));
}

void FileWatcher::Impl::add_watch(std::string_view path)
{
    if (!fs::is_dir(path))
    {
        throw std::invalid_argument(fmt::format("Is not a directory: {}", path));
    }

    _dir = CreateFile(path.data(),
                      FILE_LIST_DIRECTORY,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      NULL,
                      OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                      NULL);

    if (_dir == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error(os::last_error_str());
    }
    _overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    BOOL success = ReadDirectoryChangesW(_dir,
                                         _buffer,
                                         EVENT_BUF_LEN,
                                         TRUE,
                                         FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                                             | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE
                                             | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION
                                             | FILE_NOTIFY_CHANGE_SECURITY,
                                         NULL,
                                         &_overlapped,
                                         NULL);
    if (!success)
    {
        throw std::runtime_error(os::last_error_str());
    }

    _watch_path = path;
}

bool FileWatcher::Impl::poll_new_events(int milliseconds_timeout)
{
    if (_dir == INVALID_HANDLE_VALUE || _overlapped.hEvent == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD result = WaitForSingleObject(_overlapped.hEvent, milliseconds_timeout);
    if (result != WAIT_OBJECT_0)
    {
        return false;
    }

    DWORD bytes_transferred;
    GetOverlappedResult(_dir, &_overlapped, &bytes_transferred, FALSE);

    std::string old_filename;
    FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)_buffer;
    while (event != nullptr)
    {
        bool add_event = true;
        FileWatcherEvent fw_event;
        fw_event.watch_path = _watch_path;

        std::wstring ws(event->FileName, event->FileNameLength / sizeof(wchar_t));
        fw_event.filename = std::string(ws.begin(), ws.end());
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

    BOOL success = ReadDirectoryChangesW(_dir,
                                         _buffer,
                                         EVENT_BUF_LEN,
                                         TRUE,
                                         FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                                             | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE
                                             | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION
                                             | FILE_NOTIFY_CHANGE_SECURITY,
                                         NULL,
                                         &_overlapped,
                                         NULL);
    if (!success)
    {
        _events.emplace_back(FileWatcherEvent {.type = FileWatcherEventType::terminated, .watch_path = _watch_path});
        this->terminate();
    }

    return true;
}

void FileWatcher::Impl::terminate()
{
    if (_dir == INVALID_HANDLE_VALUE)
    {
        CloseHandle(_dir);
        _dir = INVALID_HANDLE_VALUE;
    }

    if (_overlapped.hEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_overlapped.hEvent);
        memset(&_overlapped, 0, sizeof(_overlapped));
    }
}

#else

void FileWatcher::Impl::init()
{
    _fd = inotify_init();
    if (_fd < 0)
    {
        throw std::runtime_error(strerror(errno));
    }
    _poll.set_limit(1);
    _poll.set_read_fd(_fd);
    _poll.add_observer(this);
}

void FileWatcher::Impl::add_watch(std::string_view path)
{
    _wd = inotify_add_watch(_fd, path.data(), IN_ALL_EVENTS);
    if (_wd < 0)
    {
        throw std::runtime_error(strerror(errno));
    }
    _watch_path = path;
}

bool FileWatcher::Impl::poll_new_events(int milliseconds_timeout)
{
    return _fd >= 0 && _poll.poll(milliseconds_timeout) > 0;
}

void FileWatcher::Impl::terminate()
{
    if (_wd >= 0)
    {
        inotify_rm_watch(_fd, _wd);
        _wd = -1;
    }
    if (_fd >= 0)
    {
        close(_fd);
        _poll.clear_fd(_fd);
        _fd = -1;
    }
}

void FileWatcher::Impl::handle(Poll *poll)
{
    for (const auto & event : poll->events())
    {
        bool do_termination = false;
        if (event.readable)
        {
            int length = read(_fd, _buffer, EVENT_BUF_LEN);
            if (length < 0)
            {
                SIHD_LOG(error, "FileWatcher: read error {}", strerror(errno));
                return;
            }
            std::string old_filename;
            int i = 0;
            while (i < length)
            {
                struct inotify_event *event = (struct inotify_event *)&_buffer[i];
                if (event->wd == _wd)
                {
                    bool add_event = true;
                    FileWatcherEvent fw_event;
                    fw_event.watch_path = _watch_path;

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
                    else if (event->mask & IN_DELETE_SELF)
                    {
                        fw_event.type = FileWatcherEventType::terminated;
                        do_termination = true;
                    }
                    else if (event->mask & IN_MOVE_SELF)
                    {
                        fw_event.type = FileWatcherEventType::terminated;
                        do_termination = true;
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
                        old_filename = std::move(fw_event.filename);
                        add_event = false;
                    }
                    else if (event->mask & IN_MOVED_TO)
                    {
                        fw_event.type = FileWatcherEventType::renamed;
                        fw_event.old_filename = std::move(old_filename);
                        old_filename.clear();
                    }
                    else
                    {
                        add_event = false;
                    }

                    if (add_event)
                    {
                        _events.emplace_back(std::move(fw_event));
                    }
                }
                i += EVENT_SIZE + event->len;
            }
        }
        if (do_termination || event.closed || event.error)
        {
            _events.emplace_back(
                FileWatcherEvent {.type = FileWatcherEventType::terminated, .watch_path = _watch_path});
            this->terminate();
        }
    }
}

#endif

FileWatcher::FileWatcher(std::string_view path)
{
    _impl = std::make_unique<FileWatcher::Impl>(_events);
    _impl->init();
    _impl->add_watch(path);
}

const std::vector<FileWatcherEvent> & FileWatcher::events() const
{
    return _events;
}

FileWatcher::~FileWatcher()
{
    if (_impl)
    {
        _impl->terminate();
    }
}

bool FileWatcher::check_for_changes(int milliseconds_timeout)
{
    _events.clear();
    const bool success = _impl->poll_new_events(milliseconds_timeout);
    if (success)
        this->notify_observers(this);
    return success;
}

} // namespace sihd::util
