#include <sihd/sys/platform.hpp>

#include <sihd/sys/FileWatcher.hpp>
#include <sihd/util/Logger.hpp>

// FileWatcher::Impl and the methods touching it live in src/linux|windows/FileWatcher.cpp:
// backends share no common shape (inotify + Poll vs ReadDirectoryChangesW + OVERLAPPED)

namespace sihd::sys
{

using namespace sihd::util;

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
#if defined(__SIHD_UNIX__)
        // emitted only by the inotify backend
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

void FileWatcher::set_run_timeout(int milliseconds)
{
    _run_timeout_milliseconds = milliseconds;
}

const std::vector<FileWatcherEvent> & FileWatcher::events() const
{
    return _events;
}

} // namespace sihd::sys
