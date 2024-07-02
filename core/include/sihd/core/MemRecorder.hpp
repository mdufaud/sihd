#ifndef __SIHD_CORE_MEMRECORDER_HPP__
#define __SIHD_CORE_MEMRECORDER_HPP__

#include <nlohmann/json_fwd.hpp>

#include <sihd/util/IHandler.hpp>
#include <sihd/util/IProvider.hpp>

#include <sihd/core/ACoreObject.hpp>
#include <sihd/core/Channel.hpp>
#include <sihd/core/Records.hpp>

namespace sihd::core
{

class MemRecorder: public ACoreObject,
                   public sihd::util::IProvider<PlayableRecord>,
                   public sihd::util::IHandler<const std::string &, const Channel *>
{
    public:
        using ACoreObject::handle;

        MemRecorder(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~MemRecorder();

        bool set_stop_providing_when_empty(bool active);

        bool do_start() override;
        bool do_stop() override;
        bool do_reset() override;

        void add_record(const PlayableRecord & record);
        void add_record(const std::string & name, sihd::util::Timestamp timestamp, const sihd::util::IArray *array);
        void add_records(const std::vector<PlayableRecord> & records);
        void add_records(const std::list<PlayableRecord> & records);

        bool empty() const;
        bool providing() const override;
        bool provide(PlayableRecord *value) override;

        bool is_running() const override;

        const SortedRecordedValues & sorted_recorded_values() const { return _map_sorted_records; }
        std::string hexdump_records(std::string_view separation_cols = " ", char separation_data = ' ');

        MapListRecordedValues make_recorded_values() const;
        static std::string hexdump_recorded_values(const MapListRecordedValues & recorded_values);

        void clear();

    protected:
        void handle(const std::string & name, const Channel *array) override;

    private:
        bool _stop_providing_when_empty;
        std::atomic<bool> _providing;
        std::atomic<bool> _running;
        mutable std::mutex _mutex;
        SortedRecordedValues _map_sorted_records;
};

} // namespace sihd::core

#endif