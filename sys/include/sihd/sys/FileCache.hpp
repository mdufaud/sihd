#ifndef __SIHD_SYS_FILECACHE_HPP__
#define __SIHD_SYS_FILECACHE_HPP__

#include <sihd/sys/FileWatcher.hpp>
#include <sihd/util/Cache.hpp>
#include <sihd/util/Handler.hpp>

namespace sihd::sys
{

class FileCache: public sihd::util::IHandler<FileWatcher *>
{
    public:
        using FilePath = std::string;
        using FileContent = std::string;
        using FileContentCache = sihd::util::Cache<FilePath, FileContent>;
        using OptionalCachedValue = FileContentCache::OptionalCachedValue;

        FileCache();
        virtual ~FileCache();

        void add(const std::string & file_path, bool lazy = false);
        void remove(const std::string & file_path);
        void clear();

        OptionalCachedValue get(const std::string & file_path);

    protected:
        void handle(FileWatcher *file_watcher) override;

    private:
        FileContentCache _cache;
        FileWatcher _file_watcher;
};

} // namespace sihd::sys

#endif
