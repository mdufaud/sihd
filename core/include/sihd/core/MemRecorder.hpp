#ifndef __SIHD_CORE_MEMRECORDER_HPP__
# define __SIHD_CORE_MEMRECORDER_HPP__

# include <sihd/util/IHandler.hpp>
# include <sihd/util/Providers.hpp>
# include <sihd/core/Channel.hpp>
# include <sihd/core/Records.hpp>
# include <sihd/core/ACoreObject.hpp>
# include <nlohmann/json.hpp>

namespace sihd::core
{

class MemRecorder:  public ACoreObject,
                    public sihd::util::AProvider<PlayableRecord>,
                    public sihd::util::IHandler<const std::string &, const Channel *>
{
    public:
        using ACoreObject::handle;

        MemRecorder(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~MemRecorder();

        bool do_start() override;
        bool do_stop() override;
        bool do_reset() override;

        bool set_provider(bool active);
        bool set_recorder(bool active);

        void add_record(const PlayableRecord & record);
        void add_record(const std::string & name, time_t timestamp, const sihd::util::IArray *array);
        void add_records(const std::vector<PlayableRecord> & records);
        void add_records(const std::list<PlayableRecord> & records);

        bool provider_empty() const override;
        bool providing() const override;
        bool provide(PlayableRecord *value) override;

        std::string hexdump_records();
        std::string hexdump_timeline(std::string_view separation_cols = "\t", char separation_data = ' ');
        void clear();

        const MapListRecordedValues recorded_values() const { return _map_record; }
        const SortedRecordedValues sorted_recorded_values() const { return _map_sorted_records; }

    protected:
        void handle(const std::string & name, const Channel *array) override;

    private:
        bool _provides;
        bool _records;
        std::atomic<bool> _running;
        MapListRecordedValues _map_record;
        SortedRecordedValues _map_sorted_records;
};

}

#endif