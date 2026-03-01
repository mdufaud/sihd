#include <sihd/util/Logger.hpp>
#include <sihd/util/Message.hpp>
#include <sihd/util/MessageField.hpp>

namespace sihd::util
{

SIHD_LOGGER;

Message::Message(const std::string & name, Node *parent): Node(name, parent)
{
    _total_size = 0;
    _finished = false;
}

bool Message::on_check_link(const std::string & name, Named *child)
{
    IMessageField *field = dynamic_cast<IMessageField *>(child);
    if (field == nullptr)
        SIHD_LOG(error, "Message: cannot add child {} - is not a IMessageField", name);
    return field != nullptr;
}

bool Message::on_add_child(const std::string & name, Named *child)
{
    IMessageField *field = dynamic_cast<IMessageField *>(child);
    if (field != nullptr)
        _fields[name] = field;
    return field != nullptr;
}

void Message::on_remove_child(const std::string & name, Named *child)
{
    IMessageField *field = dynamic_cast<IMessageField *>(child);
    if (field != nullptr)
        _fields.erase(name);
}

bool Message::finish()
{
    if (this->is_finished())
        return false;
    _arr.resize(_total_size);
    __assign_arr_at = 0;
    _finished = this->field_assign_buffer(_arr.buf());
    return true;
}

bool Message::field_resize(size_t size)
{
    (void)size;
    return true;
}

bool Message::_assign_field_array(IMessageField *field)
{
    bool ret = field->field_assign_buffer(_arr.buf() + __assign_arr_at);
    if (ret)
    {
        __assign_arr_at += field->field_byte_size();
        if (__assign_arr_at > _total_size)
        {
            SIHD_LOG_ERROR("Message: for {} total size {} is inferior to calculated size {}",
                           this->name(),
                           _total_size,
                           __assign_arr_at);
            return false;
        }
    }
    return ret;
}

bool Message::field_assign_buffer(void *buffer)
{
    if (_arr.assign_bytes(buffer, this->field_byte_size()) == false)
        return false;
    __assign_arr_at = 0;
    for (const auto & name : this->children_keys())
    {
        IMessageField *field = _fields.at(name);
        if (this->_assign_field_array(field) == false)
        {
            SIHD_LOG_ERROR("Message: cannot assign buffer to field '{}'", name);
            return false;
        }
    }
    return true;
}

bool Message::field_read_from(const void *buffer, size_t size)
{
    return _arr.copy_from_bytes(buffer, size);
}

bool Message::field_write_to(void *buffer, size_t size)
{
    size = std::min(size, _arr.size());
    return memcpy(buffer, _arr.buf(), size) != nullptr;
}

bool Message::add_field(const std::string & name, IMessageField *msg)
{
    Named *named = dynamic_cast<Named *>(msg);
    if (named == nullptr)
    {
        SIHD_LOG(error, "Message: cannot add '{}' as a child - not a Named object", name);
        return false;
    }
    return this->add_child(name, named, true) && this->_add_field_size(msg);
}

bool Message::add_field(const std::string & name, Type dt, size_t size)
{
    MessageField *field = this->add_child<MessageField>(name);
    bool ret = field != nullptr;
    ret = ret && field->build_array(dt, size);
    ret = ret && this->_add_field_size(field);
    return ret;
}

bool Message::_add_field_size(IMessageField *field)
{
    _total_size += field->field_byte_size();
    return true;
}

IMessageField *Message::clone() const
{
    bool error = false;
    Message *cloned = new Message(this->name());
    for (const auto & name : this->children_keys())
    {
        IMessageField *cloned_field = _fields.at(name)->clone();
        if (cloned_field != nullptr)
            cloned->add_field(name, cloned_field);
        else
        {
            SIHD_LOG(error, "Message: clone failed for field '{}'", name);
            error = true;
            break;
        }
    }
    if (!error && this->is_finished())
        error = cloned->finish() == false;
    if (error)
    {
        delete cloned;
        cloned = nullptr;
    }
    return cloned;
}

std::string Message::description() const
{
    return fmt::format("{} bytes", this->field_byte_size());
}

} // namespace sihd::util
