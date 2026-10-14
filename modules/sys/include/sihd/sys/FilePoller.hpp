#ifndef __SIHD_SYS_FILEPOLLER_HPP__
#define __SIHD_SYS_FILEPOLLER_HPP__

#include <memory>
#include <string>
#include <vector>

#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys
{

class FilePoller: public sihd::util::Observable<FilePoller>,
                  public sihd::util::IRunnable
{
    public:
        FilePoller();
        FilePoller(const std::string & path, size_t max_depth);
        virtual ~FilePoller();

        sihd::util::Result<void> watch(const std::string & path, size_t max_depth);
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

} // namespace sihd::sys

#endif
