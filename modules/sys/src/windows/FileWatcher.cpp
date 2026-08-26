#include <sihd/sys/platform.hpp>

#include <cstring>
#include <list>
#include <stdexcept>

#include <sihd/sys/FileWatcher.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#include <windows.h>

#define EVENT_SIZE (sizeof(FILE_NOTIFY_INFORMATION))
#define EVENT_BUFFER_LEN (20 * (EVENT_SIZE + MAX_PATH))

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

struct FileWatcher::Impl
{
        struct Watcher
        {
                Watcher() { memset(&overlapped, 0, sizeof(overlapped)); }
                ~Watcher() { this->close(); }

                Watcher(Watcher && other) noexcept { *this = std::move(other); }

                Watcher & operator=(Watcher && other) noexcept
                {
                    if (this != &other)
                    {
                        this->close();
                        path = std::move(other.path);
                        handle = other.handle;
                        other.handle = INVALID_HANDLE_VALUE;
                        memcpy(&overlapped, &other.overlapped, sizeof(overlapped));
                        memset(&other.overlapped, 0, sizeof(other.overlapped));
                        filename_filter = std::move(other.filename_filter);
                    }
                    return *this;
                }

                void close()
                {
                    if (handle != INVALID_HANDLE_VALUE)
                    {
                        CloseHandle(handle);
                        handle = INVALID_HANDLE_VALUE;
                    }

                    if (overlapped.hEvent != NULL)
                    {
                        CloseHandle(overlapped.hEvent);
                        memset(&overlapped, 0, sizeof(overlapped));
                    }
                }

                std::string path;
                HANDLE handle = INVALID_HANDLE_VALUE;
                OVERLAPPED overlapped {};
                // empty when watching a directory, else the file name to filter on (parent dir is watched)
                std::string filename_filter;
        };

        Impl(std::vector<FileWatcherEvent> & events): _events(events)
        {
            _buffer.resize(EVENT_BUFFER_LEN);
        }
        ~Impl() { this->terminate(); }

        std::string _buffer;
        std::vector<FileWatcherEvent> & _events;

        std::list<Watcher> _watchers;

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

void FileWatcher::Impl::init() {}

bool FileWatcher::Impl::add_watch(std::string_view path)
{
    if (this->is_watching(path))
        return true;

    // ReadDirectoryChangesW only watches directories: when given a file, watch its
    // parent directory and filter incoming events on the file name.
    std::string filename_filter;
    std::string dir_to_watch(path);
    if (!fs::is_dir(path))
    {
        if (!fs::is_file(path))
        {
            SIHD_LOG(error, "FileWatcher: No such file or directory: {}", path);
            return false;
        }
        filename_filter = fs::filename(path);
        dir_to_watch = fs::parent(path);
    }

    HANDLE handle = CreateFile(dir_to_watch.c_str(),
                               FILE_LIST_DIRECTORY,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL,
                               OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                               NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        SIHD_LOG(error, "FileWatcher: {}", os::last_error_str());
        return false;
    }

    // ReadDirectoryChangesW is asynchronous: the OVERLAPPED it is given must keep a stable address
    // until the operation completes. Store the Watcher first (std::list nodes never move) so the OS
    // never ends up writing the completion result into freed/moved memory.
    Watcher & watcher = _watchers.emplace_back();
    watcher.path = std::string(path);
    watcher.filename_filter = std::move(filename_filter);
    watcher.handle = handle;
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
        _watchers.pop_back();
        return false;
    }

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

            // when watching a single file, drop events for the other entries of the parent directory
            if (!watcher.filename_filter.empty() && !str::iequals(fw_event.filename, watcher.filename_filter))
                add_event = false;

            if (add_event)
            {
                _events.emplace_back(std::move(fw_event));
            }

            if (event->NextEntryOffset == 0)
            {
                break;
            }
            event = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(
                reinterpret_cast<uint8_t *>(event) + event->NextEntryOffset);
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
