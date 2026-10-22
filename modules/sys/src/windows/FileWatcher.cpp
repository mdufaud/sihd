#include <windows.h>

#include <array>
#include <cstring>
#include <list>
#include <stdexcept>

#include <sihd/sys/FileWatcher.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/str.hpp>

#define EVENT_SIZE (sizeof(FILE_NOTIFY_INFORMATION))
#define EVENT_BUFFER_LEN (20 * (EVENT_SIZE + MAX_PATH))

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

SIHD_LOGGER;

constexpr DWORD cancel_timeout_ms = 1000;

struct FileWatcher::Impl
{
        struct Watcher
        {
                Watcher() { memset(&overlapped, 0, sizeof(overlapped)); }
                ~Watcher() { this->close(); }

                Watcher(const Watcher &) = delete;
                Watcher & operator=(const Watcher &) = delete;

                bool start();

                void close()
                {
                    if (handle != INVALID_HANDLE_VALUE)
                    {
                        if (pending)
                        {
                            CancelIoEx(handle, &overlapped);
                            // the kernel may still write the buffer: wait for the cancelled read completion
                            (void)WaitForSingleObject(overlapped.hEvent, cancel_timeout_ms);
                            pending = false;
                        }
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
                alignas(DWORD) std::array<char, EVENT_BUFFER_LEN> buffer {};
                bool pending = false;
                // empty when watching a directory, else the file name to filter on (parent dir is watched)
                std::string filename_filter;
                std::string old_filename;
        };

        Impl(std::vector<FileWatcherEvent> & events): _events(events) {}
        ~Impl() { this->terminate(); }

        std::vector<FileWatcherEvent> & _events;

        std::list<Watcher> _watchers;

        void init();
        Result<void> add_watch(const std::string & path);
        bool rm_watch(const std::string & path);
        bool is_watching(const std::string & path);
        void terminate();
        bool poll_new_events(int milliseconds_timeout);
};

bool FileWatcher::Impl::Watcher::start()
{
    pending = ReadDirectoryChangesW(handle,
                                    buffer.data(),
                                    sizeof(buffer),
                                    FALSE,
                                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                                        | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE
                                        | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION
                                        | FILE_NOTIFY_CHANGE_SECURITY,
                                    nullptr,
                                    &overlapped,
                                    nullptr)
              != 0;
    return pending;
}

bool FileWatcher::Impl::is_watching(const std::string & path)
{
    return std::find_if(_watchers.begin(), _watchers.end(), [path](const Watcher & w) { return w.path == path; })
           != _watchers.end();
}

bool FileWatcher::Impl::rm_watch(const std::string & path)
{
    auto it = std::find_if(_watchers.begin(), _watchers.end(), [path](const Watcher & w) { return w.path == path; });
    const bool found = it != _watchers.end();
    if (found)
    {
        it->close();
        _watchers.erase(it);
    }
    return found;
}

void FileWatcher::Impl::init() {}

Result<void> FileWatcher::Impl::add_watch(const std::string & path)
{
    if (this->is_watching(path))
        return {};

    // ReadDirectoryChangesW only watches directories: for a file, watch its parent and filter on the file name
    std::string filename_filter;
    std::string dir_to_watch(path);
    const DWORD attributes = GetFileAttributesW(str::to_wstr(path).c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return std::unexpected(Error(not_found, "could not watch '{}': {}", path, os::last_error_str()));
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        filename_filter = fs::filename(path);
        dir_to_watch = fs::parent(path);
    }

    HANDLE handle = CreateFileW(str::to_wstr(dir_to_watch).c_str(),
                                FILE_LIST_DIRECTORY,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return std::unexpected(Error(io_error, "could not watch '{}': {}", path, os::last_error_str()));

    // OVERLAPPED address must stay stable until completion: watcher is stored first (std::list nodes never move)
    Watcher & watcher = _watchers.emplace_back();
    watcher.path = std::string(path);
    watcher.filename_filter = std::move(filename_filter);
    watcher.handle = handle;
    watcher.overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (watcher.overlapped.hEvent == nullptr || !watcher.start())
    {
        auto error = Error(io_error, "could not watch '{}': {}", path, os::last_error_str());
        _watchers.pop_back();
        return std::unexpected(std::move(error));
    }

    return {};
}

bool FileWatcher::Impl::poll_new_events(int milliseconds_timeout)
{
    std::list<std::string> watchers_to_remove;

    for (auto & watcher : _watchers)
    {
        DWORD result = WaitForSingleObject(watcher.overlapped.hEvent, milliseconds_timeout);
        if (result != WAIT_OBJECT_0)
        {
            continue;
        }

        DWORD bytes_transferred = 0;
        if (GetOverlappedResult(watcher.handle, &watcher.overlapped, &bytes_transferred, FALSE) == 0)
        {
            SIHD_LOG(error, "FileWatcher: could not get overlapped result: {}", os::last_error_str());
            continue;
        }
        watcher.pending = false;
        if (bytes_transferred == 0)
            _events.emplace_back(FileWatcherEvent {.type = FileWatcherEventType::overflow, .watch_path = watcher.path});

        std::string & old_filename = watcher.old_filename;
        FILE_NOTIFY_INFORMATION *event = bytes_transferred == 0 ? nullptr
                                                                : (FILE_NOTIFY_INFORMATION *)watcher.buffer.data();
        while (event != nullptr)
        {
            FileWatcherEvent fw_event;
            fw_event.watch_path = watcher.path;

            fw_event.filename = str::to_str(
                std::wstring_view(event->FileName, event->FileNameLength / sizeof(wchar_t)));
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

            if (!watcher.filename_filter.empty() && !str::iequals(fw_event.filename, watcher.filename_filter)
                && !str::iequals(fw_event.old_filename, watcher.filename_filter))
                add_event = false;

            if (add_event)
            {
                _events.emplace_back(std::move(fw_event));
            }

            if (event->NextEntryOffset == 0)
            {
                break;
            }
            event = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(reinterpret_cast<uint8_t *>(event)
                                                                + event->NextEntryOffset);
        }

        if (!watcher.start())
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

FileWatcher::FileWatcher(const std::string & path): FileWatcher()
{
    auto watched = _impl->add_watch(path);
    if (!watched)
        throw std::runtime_error(watched.error().message);
}

FileWatcher::FileWatcher(const std::string & path, int run_timeout_milliseconds): FileWatcher(path)
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

Result<void> FileWatcher::watch(const std::string & path)
{
    return _impl->add_watch(path);
}

bool FileWatcher::unwatch(const std::string & path)
{
    return _impl->rm_watch(path);
}

bool FileWatcher::is_watching(const std::string & path) const
{
    return _impl->is_watching(path);
}

void FileWatcher::clear()
{
    _impl->terminate();
}

} // namespace sihd::sys
