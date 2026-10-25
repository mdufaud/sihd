#ifndef __SIHD_SYS_FILEWATCHER_HPP__
#define __SIHD_SYS_FILEWATCHER_HPP__

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

enum class FileWatcherEventType
{
    unknown = 0,
    created,
    deleted,
    modified,
    renamed,
    terminated,
#if defined(__SIHD_UNIX__)
    opened,
    accessed,
    closed,
#endif
    overflow,
};

struct FileWatcherEvent
{
        FileWatcherEventType type;
        std::string watch_path;
        std::string filename = "";
        std::string old_filename = "";

        std::string type_str() const;
};

class FileWatcher: public sihd::util::Observable<FileWatcher>,
                   public sihd::util::IRunnable
{
    public:
        static constexpr bool supported = !sihd::util::build::is_emscripten;

        FileWatcher();
        FileWatcher(const std::string & path);
        FileWatcher(const std::string & path, sihd::util::Duration run_timeout);
        virtual ~FileWatcher();

        void set_run_timeout(sihd::util::Duration timeout);

        sihd::util::Result<void> watch(const std::string & path);
        bool unwatch(const std::string & path);
        bool is_watching(const std::string & path) const;
        void clear();

        bool run() override;

        const std::vector<FileWatcherEvent> & events() const;

    protected:

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
        sihd::util::Duration _run_timeout = std::chrono::milliseconds(100);
        std::vector<FileWatcherEvent> _events;
};

} // namespace sihd::sys

#endif
