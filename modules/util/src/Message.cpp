#include <sihd/util/Logger.hpp>
#include <sihd/util/Message.hpp>
#include <sihd/util/MessageField.hpp>
#include <sihd/util/NamedFactory.hpp>

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
    (void)name;
    IMessageField *field = dynamic_cast<IMessageField *>(child);
    return field != nullptr;
}

bool Message::on_add_child(const std::string & name, Named *child)
{
    IMessageField *field = dynamic_cast<IMessageField *>(child);
    if (field != nullptr)
    {
        _fields[name] = field;
        this->_update_field_size();
    }
    return field != nullptr;
}

void Message::on_remove_child(const std::string & name, Named *child)
{
    IMessageField *field = dynamic_cast<IMessageField *>(child);
    if (field != nullptr)
    {
        _fields.erase(name);
        this->_update_field_size();
    }
}

bool Message::finish()
{
    if (this->is_finished())
        return false;
    this->_update_field_size();
    if (!_arr.resize(_total_size))
        return false;
    __assign_arr_at = 0;
    _finished = this->field_assign_buffer(_arr.buf());
    return _finished;
}

bool Message::field_resize(size_t size)
{
    (void)size;
    return true;
}

bool Message::_assign_field_array(IMessageField *field)
{
    if (field->field_byte_size() > _total_size - __assign_arr_at)
        return false;
    bool ret = field->field_assign_buffer(_arr.buf() + __assign_arr_at);
    if (ret)
    {
        __assign_arr_at += field->field_byte_size();
        if (__assign_arr_at > _total_size)
        {
            SIHD_LOG(error,
                     "Message: for {} total size {} is inferior to calculated size {}",
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
            SIHD_LOG(error, "Message: cannot assign buffer to field '{}'", name);
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
    if (SIHD_UNEXPECTED_LOG(this->add_child(name, named, true)))
        return false;
    return true;
}

bool Message::add_field(const std::string & name, Type dt, size_t size)
{
    MessageField *field = this->add_child<MessageField>(name);
    bool ret = field != nullptr;
    ret = ret && field->build_array(dt, size);
    if (ret)
        this->_update_field_size();
    return ret;
}

void Message::_update_field_size()
{
    _total_size = 0;
    for (const auto & [name, field] : _fields)
        _total_size += field->field_byte_size();
}

IMessageField *Message::clone() const
{
    bool error = false;
    Message *cloned = new Message(this->name());
    for (const auto & name : this->children_keys())
    {
        IMessageField *cloned_field = _fields.at(name)->clone();
        if (cloned_field != nullptr)
        {
            if (cloned->add_field(name, cloned_field) == false)
            {
                SIHD_LOG(error, "Message: could not add field '{}' to the clone", name);
                delete cloned_field;
                error = true;
                break;
            }
        }
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

const MessageField *Message::_require_field(const std::string & name) const
{
    const MessageField *field = this->get_child<MessageField>(name);
    if (field == nullptr)
        throw std::invalid_argument("Message: no such field '" + name + "'");
    return field;
}

std::vector<uint8_t> Message::to_bytes()
{
    const size_t sz = this->field_byte_size();
    std::vector<uint8_t> result(sz);
    if (this->field_write_to(result.data(), sz) == false)
        SIHD_LOG(error, "Message: could not write fields to bytes");
    return result;
}

bool Message::from_bytes(std::span<const uint8_t> data)
{
    return this->field_read_from(data.data(), data.size());
}

SIHD_REGISTER_FACTORY(Message);

} // namespace sihd::util
