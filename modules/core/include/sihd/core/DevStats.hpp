#ifndef __SIHD_CORE_DEVSTATS_HPP__
#define __SIHD_CORE_DEVSTATS_HPP__

#include <map>
#include <vector>

#include <sihd/core/Device.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/SafeQueue.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::core
{

// every linked channel NAME gets NAME_count, NAME_rate, NAME_bytes, NAME_byte_rate, NAME_min, NAME_max, NAME_mean
class DevStats: public sihd::core::Device,
                protected sihd::util::IRunnable
{
    public:
        DevStats(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevStats();

        bool is_running() const override;

        bool set_period(int milliseconds);

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *c) override;

        bool run() override;

        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        struct Sample
        {
                Channel *in;
                size_t bytes;
                std::vector<double> values;
        };

        struct Lane
        {
                sihd::util::Stat<double> values;
                uint64_t count = 0;
                uint64_t bytes = 0;
                Channel *count_ptr = nullptr;
                Channel *rate_ptr = nullptr;
                Channel *bytes_ptr = nullptr;
                Channel *byte_rate_ptr = nullptr;
                Channel *min_ptr = nullptr;
                Channel *max_ptr = nullptr;
                Channel *mean_ptr = nullptr;
        };

        bool _bind_lane(const std::string & name, Lane & lane);
        Channel *_output(const std::string & name, sihd::util::Type type);
        void _accumulate();

        sihd::util::StepWorker _step_worker;
        sihd::util::SteadyClock _clock;
        sihd::util::Timestamp _start_time;
        sihd::util::SafeQueue<Sample> _pending;
        std::map<Channel *, Lane> _lanes;
};

} // namespace sihd::core

#endif
