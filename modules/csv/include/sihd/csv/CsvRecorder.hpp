#ifndef __SIHD_CSV_CSVRECORDER_HPP__
#define __SIHD_CSV_CSVRECORDER_HPP__

#include <mutex>
#include <string>

#include <sihd/core/ACoreObject.hpp>
#include <sihd/core/Channel.hpp>
#include <sihd/csv/CsvWriter.hpp>
#include <sihd/util/IHandler.hpp>

namespace sihd::csv
{

// a DevRecorder handler: each record is the row "timestamp,channel,value"
class CsvRecorder: public sihd::core::ACoreObject,
                   public sihd::util::IHandler<const std::string &, const sihd::core::Channel *>
{
    public:
        using ACoreObject::handle;

        CsvRecorder(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~CsvRecorder();

        bool set_path(const std::string & path);

        bool is_running() const override;

    protected:
        bool do_start() override;
        bool do_stop() override;

        void handle(const std::string & name, const sihd::core::Channel *channel) override;

    private:
        std::string _path;
        mutable std::mutex _writer_mutex;
        CsvWriter _writer;
};

} // namespace sihd::csv

#endif
