#ifndef __SIHD_CORE_DEVPULSATION_HPP__
# define __SIHD_CORE_DEVPULSATION_HPP__

# include <sihd/core/Device.hpp>
# include <sihd/util/Scheduler.hpp>
# include <sihd/util/IRunnable.hpp>

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
        bool _running;
        double _frequency;
        uint32_t _beats;
        sihd::util::Scheduler _scheduler;
        Channel *_channel_heartbeat_ptr;
        Channel *_channel_activate_ptr;
        std::mutex _mutex;
};

}

#endif