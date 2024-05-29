#ifndef __SIHD_CORE_DEVRECORDER_HPP__
#define __SIHD_CORE_DEVRECORDER_HPP__

#include <list>
#include <memory>
#include <set>

#include <sihd/util/IHandler.hpp>
#include <sihd/util/forward.hpp>

#include <sihd/core/Device.hpp>

namespace sihd::core
{

class DevRecorder: public sihd::core::Device
{
    public:
        DevRecorder(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevRecorder();

        bool is_running() const override;

        // gets at init() the recorder pointing to path
        bool set_handler(std::string_view path);
        // expects conf: ALIAS=CHANNEL_NAME
        bool add_record_channel(std::string_view conf);
        // removes channel from alias
        bool remove_recorded_channel(const std::string & alias);

    protected:
        void handle(sihd::core::Channel *c) override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        std::mutex _run_mutex;
        std::mutex _mutex_recorded_values;
        // list of channels path to get at start
        std::map<std::string, std::string> _map_channels_alias;
        // corresponding channel ptr to its recorded path
        std::map<Channel *, std::string> _map_channels;

        bool _running;
        std::string _handler_path;
        std::unique_ptr<sihd::util::ArrUInt> _records_array_ptr;
        // sihd::util::ArrUInt _records_array;
        sihd::util::IHandler<const std::string &, const Channel *> *_handler_ptr;

        Channel *_channel_records_ptr;
};

} // namespace sihd::core

#endif