#ifndef __SIHD_UTIL_FILEWATCHER_HPP__
#define __SIHD_UTIL_FILEWATCHER_HPP__

#include <functional>
#include <string>
#include <string_view>

#include <sihd/util/Poll.hpp>

namespace sihd::util
{

enum FileWatcherEventType
{
    created,
    deleted,
    modified,
    renamed
};

struct FileWatcherEvent
{
        std::string_view path;
        FileWatcherEventType type;
};

class FileWatcher: public sihd::util::IHandler<sihd::util::Poll *>
{
    public:
        using Callback = std::function<void(FileWatcherEvent)>;

        FileWatcher(std::string_view path, Callback && callback);
        virtual ~FileWatcher();

        bool poll();

    protected:
        void handle(Poll *poll);
        void rm_watch();

    private:

        int _fd;
        int _wd;
        std::string _path;
        Callback _callback;
        Poll _poll;
};

} // namespace sihd::util

#endif
