#ifndef __SIHD_CORE_DEVBATCHER_HPP__
#define __SIHD_CORE_DEVBATCHER_HPP__

#include <map>

#include <sihd/core/Device.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/SafeQueue.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::core
{

// every linked channel NAME is batched on NAME_batch
class DevBatcher: public sihd::core::Device,
                  protected sihd::util::IRunnable
{
    public:
        DevBatcher(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevBatcher();

        bool is_running() const override;

        bool set_samples(size_t samples);
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
                sihd::util::ArrByte data;
                sihd::util::Timestamp arrival;
        };

        struct Block
        {
                sihd::util::ArrByte data;
                sihd::util::Timestamp arrival;
                size_t samples = 0;
        };

        struct Lane
        {
                Channel *batch = nullptr;
                Block current;
        };

        void _accumulate();
        void _write_current(Lane & lane);

        size_t _samples;
        sihd::util::Duration _period;
        sihd::util::SteadyClock _clock;
        sihd::util::StepWorker _step_worker;
        sihd::util::SafeQueue<Sample> _pending;
        std::map<Channel *, Lane> _lanes;
};

} // namespace sihd::core

#endif
