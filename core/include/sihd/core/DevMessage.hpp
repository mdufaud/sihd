#ifndef __SIHD_CORE_DEVMESSAGE_HPP__
#define __SIHD_CORE_DEVMESSAGE_HPP__

#include <sihd/util/IMessageField.hpp>

#include <sihd/core/Device.hpp>

namespace sihd::core
{

class DevMessage: public sihd::core::Device
{
    public:
        DevMessage(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevMessage();

        bool is_running() const override;
        bool set_message_path(std::string_view path);
        // as opposed to immediate mode - compute output on trigger only
        bool set_trigger_mode(bool active);

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *channel) override;

        bool on_setup() override;
        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        void _fill_channels_out();
        void _compute_output();

        bool _running;
        bool _trigger_mode;
        std::string _msg_path;

        sihd::util::IMessageField *_msg_ptr;
        std::map<std::string, sihd::util::IMessageField *> _msg_fields;
        std::map<sihd::util::IMessageField *, Channel *> _fields_to_channel_out;
        std::map<Channel *, sihd::util::IMessageField *> _channels_in_to_field;
        Channel *_channel_msg_in;
        Channel *_channel_msg_out;
        Channel *_channel_trigger;
};

} // namespace sihd::core

#endif