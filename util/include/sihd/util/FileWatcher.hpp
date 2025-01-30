#ifndef __SIHD_UTIL_FILEWATCHER_HPP__
#define __SIHD_UTIL_FILEWATCHER_HPP__

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/Observable.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::util
{

enum class FileWatcherEventType
{
    unknown = 0,
    created,
    deleted,
    modified,
    renamed,
    terminated, // watch is finished
#if !defined(__SIHD_WINDOWS__)
    // not available in windows
    opened,
    accessed,
    closed,
#endif
};

struct FileWatcherEvent
{
        FileWatcherEventType type;
        std::string_view watch_path;
        std::string filename = "";
        std::string old_filename = "";

        std::string type_str() const;
};

class FileWatcher: public sihd::util::Observable<FileWatcher>
{
    public:
        FileWatcher() = default; // does nothing
        FileWatcher(std::string_view path);
        virtual ~FileWatcher();

        const std::vector<FileWatcherEvent> & events() const;

        bool check_for_changes(int milliseconds_timeout = 0);

    protected:

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
        std::vector<FileWatcherEvent> _events;
};

} // namespace sihd::util

#endif
