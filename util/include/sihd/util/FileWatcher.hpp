#ifndef __SIHD_UTIL_FILEWATCHER_HPP__
#define __SIHD_UTIL_FILEWATCHER_HPP__

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/util/IRunnable.hpp>
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

class FileWatcher: public sihd::util::Observable<FileWatcher>,
                   public sihd::util::IRunnable
{
    public:
        FileWatcher();
        FileWatcher(std::string_view path);
        FileWatcher(std::string_view path, int run_timeout_milliseconds);
        virtual ~FileWatcher();

        void set_run_timeout(int milliseconds);

        bool watch(std::string_view path);
        bool unwatch(std::string_view path);
        bool is_watching(std::string_view path) const;
        void clear();

        bool run() override;

        const std::vector<FileWatcherEvent> & events() const;

    protected:

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
        int _run_timeout_milliseconds;
        std::vector<FileWatcherEvent> _events;
};

} // namespace sihd::util

#endif
