#ifndef __SIHD_UTIL_FILEPOLLER_HPP__
#define __SIHD_UTIL_FILEPOLLER_HPP__

#include <memory>
#include <string>
#include <string_view>

#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Observable.hpp>

namespace sihd::util
{

class FilePoller: public sihd::util::Observable<FilePoller>,
                  public sihd::util::IRunnable
{
    public:
        FilePoller();
        FilePoller(std::string_view path, size_t max_depth);
        virtual ~FilePoller();

        bool watch(std::string_view path, size_t max_depth);
        void unwatch();

        bool run() override;

        const std::vector<std::string> & created() const;
        const std::vector<std::string> & removed() const;
        const std::vector<std::string> & changed() const;

        size_t watch_size() const;
        const std::string & watch_path() const;

    protected:

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::util

#endif
