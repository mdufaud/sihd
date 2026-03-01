#include <sihd/core/DevMessage.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/NamedFactory.hpp>

#define CHANNEL_TRIGGER "trigger"
#define IN_SUFFIX "_in"
#define OUT_SUFFIX "_out"
#define CHANNEL_MSG_IN "buffer" IN_SUFFIX
#define CHANNEL_MSG_OUT "buffer" OUT_SUFFIX

namespace sihd::core
{

SIHD_REGISTER_FACTORY(DevMessage)

SIHD_LOGGER;

using namespace sihd::util;

DevMessage::DevMessage(const std::string & name, sihd::util::Node *parent):
    sihd::core::Device(name, parent),
    _running(false),
    _trigger_mode(false)
{
    this->add_conf("message", &DevMessage::set_message_path);
    this->add_conf("trigger_mode", &DevMessage::set_trigger_mode);
}

DevMessage::~DevMessage() = default;

bool DevMessage::set_message_path(std::string_view path)
{
    _msg_path = path;
    return true;
}

bool DevMessage::set_trigger_mode(bool active)
{
    _trigger_mode = active;
    return true;
}

bool DevMessage::is_running() const
{
    return _running;
}

void DevMessage::_compute_output()
{
    if (_msg_ptr->field_write_to(_channel_msg_out->data(), _channel_msg_out->byte_size()) == false)
    {
        SIHD_LOG(error, "DevMessage: cannot write message {}", _channel_msg_out->name());
        return;
    }
    _channel_msg_out->notify();
}

void DevMessage::_fill_channels_out()
{
    for (const auto & [field, ch_out] : _fields_to_channel_out)
    {
        if (field->field_write_to(ch_out->data(), ch_out->byte_size()) == false)
        {
            SIHD_LOG(error, "DevMessage: cannot write into channel {}", ch_out->name());
            return;
        }
        ch_out->notify();
    }
}

void DevMessage::handle(sihd::core::Channel *channel)
{
    if (channel == _channel_trigger)
    {
        if (_trigger_mode)
        {
            this->_fill_channels_out();
            this->_compute_output();
        }
        _channel_msg_out->notify();
        return;
    }
    else if (channel == _channel_msg_in)
    {
        if (_msg_ptr->field_read_from(channel->array()->buf(), channel->size()) == false)
        {
            SIHD_LOG(error, "DevMessage: cannot fill message from channel {}", _channel_msg_in->name());
            return;
        }
        if (_trigger_mode == false)
        {
            this->_fill_channels_out();
            this->_compute_output();
        }
        return;
    }
    const auto it = _channels_in_to_field.find(channel);
    if (it != _channels_in_to_field.end())
    {
        IMessageField *field = it->second;
        if (field->field_read_from(channel->array()->buf(), channel->size()) == false)
        {
            SIHD_LOG(error, "DevMessage: cannot fill field from channel {}", channel->name());
            return;
        }
        if (_trigger_mode == false)
        {
            Channel *ch_out = _fields_to_channel_out.at(field);
            if (field->field_write_to(ch_out->data(), ch_out->byte_size()) == false)
            {
                SIHD_LOG(error, "DevMessage: cannot write into channel {}", ch_out->name());
                return;
            }
            ch_out->notify();
            this->_compute_output();
        }
    }
}

bool DevMessage::on_setup()
{
    return true;
}

bool DevMessage::on_init()
{
    _msg_ptr = this->find<IMessageField>(_msg_path);
    if (_msg_ptr == nullptr)
    {
        SIHD_LOG(error, "DevMessage: no message at '{}'", _msg_path);
        return false;
    }
    if (_msg_ptr->is_finished() == false)
    {
        SIHD_LOG(error, "DevMessage: message is not finished '{}'", _msg_path);
        return false;
    }
    Node *msg_node = dynamic_cast<Node *>(_msg_ptr);
    if (msg_node != nullptr)
    {
        for (const auto & name : msg_node->children_keys())
        {
            IMessageField *field_child = msg_node->find<IMessageField>(name);
            if (field_child != nullptr)
            {
                _msg_fields.emplace(name, field_child);
                this->add_unlinked_channel(name + IN_SUFFIX,
                                           field_child->field_type(),
                                           field_child->field_size());
                this->add_unlinked_channel(name + OUT_SUFFIX,
                                           field_child->field_type(),
                                           field_child->field_size());
            }
        }
    }
    this->add_unlinked_channel(CHANNEL_TRIGGER, Type::TYPE_BOOL, 1);
    this->add_unlinked_channel(CHANNEL_MSG_IN, _msg_ptr->field_type(), _msg_ptr->field_byte_size());
    this->add_unlinked_channel(CHANNEL_MSG_OUT, _msg_ptr->field_type(), _msg_ptr->field_byte_size());
    return true;
}

bool DevMessage::on_start()
{
    Channel *c;
    bool ret = true;

    for (const auto & [name, field] : _msg_fields)
    {
        std::string suffix_name = name + IN_SUFFIX;

        ret = this->find_channel(suffix_name, &c) && ret;
        _channels_in_to_field[c] = field;
        this->observe_channel(c);

        suffix_name = name + OUT_SUFFIX;

        ret = this->find_channel(suffix_name, &c) && ret;
        _fields_to_channel_out[field] = c;
    }

    ret = this->find_channel(CHANNEL_MSG_IN, &c) && ret;
    _channel_msg_in = c;
    this->observe_channel(c);

    ret = this->find_channel(CHANNEL_MSG_OUT, &c) && ret;
    _channel_msg_out = c;

    ret = this->find_channel(CHANNEL_TRIGGER, &c) && ret;
    _channel_trigger = c;
    this->observe_channel(c);

    _running = ret;
    return ret;
}

bool DevMessage::on_stop()
{
    _running = false;
    return true;
}

bool DevMessage::on_reset()
{
    _msg_ptr = nullptr;
    _msg_fields.clear();
    _channels_in_to_field.clear();
    _channel_msg_in = nullptr;
    _channel_msg_out = nullptr;
    _channel_trigger = nullptr;
    return true;
}

} // namespace sihd::core
