#include <sihd/sys/FileCache.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

std::string read_file(const std::string & file_path)
{
    auto file_content = fs::read_all(file_path);
    // meant to throw so the cache is never updated
    return file_content.value();
}

} // namespace

FileCache::FileCache()
{
    constexpr int run_timeout_milliseconds = 1;
    _file_watcher.set_run_timeout(run_timeout_milliseconds); // no blocking, just check for events
    _file_watcher.add_observer(this);
}

FileCache::~FileCache() {}

void FileCache::add(const std::string & file_path, bool lazy)
{
    constexpr Timestamp max_age = -1;
    _cache.set(file_path, [internal_file_path = file_path]() { return read_file(internal_file_path); }, max_age, lazy);
    _file_watcher.watch(file_path);
}

void FileCache::remove(const std::string & file_path)
{
    _cache.erase(file_path);
    _file_watcher.unwatch(file_path);
}

void FileCache::clear()
{
    _cache.clear();
    _file_watcher.clear();
}

FileCache::OptionalCachedValue FileCache::get(const std::string & file_path)
{
    // will invalidate the cache if the file has been modified
    _file_watcher.run();
    try
    {
        return _cache.get(file_path);
    }
    catch (const std::bad_optional_access & e)
    {
        SIHD_LOG_ERROR("Failed to access cached file: {}", file_path);
        return std::nullopt;
    }
    // do not catch out of range
}

void FileCache::handle(FileWatcher *file_watcher)
{
    for (const auto & event : file_watcher->events())
    {
        switch (event.type)
        {
            case FileWatcherEventType::created:
            case FileWatcherEventType::deleted:
            case FileWatcherEventType::modified:
            case FileWatcherEventType::renamed:
                _cache.invalidate(event.watch_path);
                break;
            case FileWatcherEventType::unknown:
            case FileWatcherEventType::opened:
            case FileWatcherEventType::accessed:
            case FileWatcherEventType::closed:
                // do nothing, we don't care about these events
                break;
            case FileWatcherEventType::terminated:
                SIHD_LOG_DEBUG("File watcher terminated for file: {}", event.watch_path);
                break;
        }
    }
}

} // namespace sihd::sys
