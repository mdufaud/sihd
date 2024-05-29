#ifndef __SIHD_CORE_DEVSAMPLER_HPP__
#define __SIHD_CORE_DEVSAMPLER_HPP__

#include <set>

#include <sihd/util/IRunnable.hpp>
#include <sihd/util/StepWorker.hpp>

#include <sihd/core/Device.hpp>

namespace sihd::core
{

class DevSampler: public sihd::core::Device,
                  public sihd::util::IRunnable
{
    public:
        DevSampler(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevSampler();

        bool set_sample(std::string_view conf);
        bool set_frequency(double freq);

        bool is_running() const override;

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *c) override;
        bool run() override;

        bool on_setup() override;
        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        bool _running;
        std::mutex _run_mutex;
        std::mutex _set_mutex;
        sihd::util::StepWorker _step_worker;
        std::map<Channel *, Channel *> _channels_map;
        std::map<std::string, std::string> _conf_map;
        std::set<Channel *> _channels_sample_set;
};

} // namespace sihd::core

#endif