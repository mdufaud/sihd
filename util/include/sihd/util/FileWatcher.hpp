#ifndef __SIHD_UTIL_FILEWATCHER_HPP__
#define __SIHD_UTIL_FILEWATCHER_HPP__

#include <string>
#include <string_view>

#include <sihd/util/AThreadedService.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/StepWorker.hpp>

namespace sihd::util
{

class FileWatcher: public Named,
                   public AThreadedService,
                   public Configurable,
                   public sihd::util::Observable<FileWatcher>,
                   public sihd::util::IRunnable
{
    public:
        FileWatcher(const std::string & name, Node *parent = nullptr);
        virtual ~FileWatcher();

        bool set_path(std::string_view path);
        bool set_max_depth(size_t depth);
        bool set_polling_frequency(double hz);

        bool is_running() const override;

        const std::vector<std::string> & created() const;
        const std::vector<std::string> & removed() const;
        const std::vector<std::string> & changed() const;

        size_t watch_size() const;
        const std::string & watch_path() const { return _watch_path; }

    protected:
        bool on_start() override;
        bool on_stop() override;
        bool run() override;

    private:
        struct FileWatch;

        std::unique_ptr<FileWatch> _file_watch_ptr;

        StepWorker _step_worker;
        std::string _watch_path;
        uint32_t _max_depth;
};

} // namespace sihd::util

#endif
