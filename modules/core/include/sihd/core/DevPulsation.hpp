#ifndef __SIHD_CORE_DEVPULSATION_HPP__
#define __SIHD_CORE_DEVPULSATION_HPP__

#include <sihd/util/IRunnable.hpp>
#include <sihd/util/StepWorker.hpp>

#include <sihd/core/Device.hpp>

namespace sihd::core
{

class DevPulsation: public sihd::core::Device,
                    public sihd::util::IRunnable
{
    public:
        DevPulsation(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevPulsation();

        bool is_running() const override;

        bool set_frequency(double freq);

    protected:
        void handle(sihd::core::Channel *c) override;

        bool run() override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        uint32_t _beats;
        sihd::util::StepWorker _step_worker;
        Channel *_channel_heartbeat_ptr;
        Channel *_channel_activate_ptr;
};

} // namespace sihd::core

#endif