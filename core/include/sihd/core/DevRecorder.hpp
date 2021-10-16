#ifndef __SIHD_CORE_DEVRECORDER_HPP__
# define __SIHD_CORE_DEVRECORDER_HPP__

# include <sihd/core/Device.hpp>
# include <list>
# include <set>

namespace sihd::core
{

class DevRecorder:   public sihd::core::Device
{
    public:
        typedef std::pair<time_t, sihd::util::IArray *> recorded_value;
        typedef std::list<recorded_value> lst_recorded_values;
        typedef std::map<std::string, lst_recorded_values> map_recorded_channels;

        DevRecorder(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevRecorder();

        static std::string to_string(const map_recorded_channels & map);

        bool is_running() const override;

        bool add_record_channel(const std::string & path);
        bool remove_recorded_channel(const std::string & path);

        void clear_recorded_values();

        const map_recorded_channels & recorded_channels_values() const { return _map_channels_values; }

    protected:
        void observable_changed([[maybe_unused]] sihd::core::Channel *c) override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        std::mutex _mutex_recorded_values;
        // list of channels path to get at start
        std::set<std::string> _set_channels_to_record;
        // corresponding channel ptr to its recorded path
        std::map<Channel *, std::string> _map_channels;
        // a map of each channels recorded values
        map_recorded_channels _map_channels_values;

        bool _running;
        Channel *_channel_clear_ptr;
        Channel *_channel_records_ptr;
        sihd::util::ArrUInt _records_array;
};

}

#endif