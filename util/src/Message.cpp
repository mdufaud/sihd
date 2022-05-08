#include <sihd/util/Message.hpp>

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(Message);

SIHD_LOGGER;

Message::Message(const std::string & name, Node *parent): Node(name, parent)
{
    _total_size = 0;
    _finished = false;
}

Message::~Message()
{
}

IMessageField *Message::clone() const
{
    bool error = false;
    Message *cloned = new Message(this->name());
    for (const std::string & name: this->children_keys())
    {
        const IMessageField *field = this->cfind<IMessageField>(name);
        if (field != nullptr)
        {
            IMessageField *cloned_field = field->clone();
            if (cloned_field != nullptr)
                cloned->add_field(name, cloned_field);
            else
            {
                SIHD_LOG(critical, "Message: clone failed for " << name);
                error = true;
            }
        }
        else
        {
            SIHD_LOG(critical, "Message: could not clone field " << name << " - not a IMessageField");
            error = true;
        }
        if (error)
            break ;
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

bool    Message::assign_field_buffer(uint8_t *buffer)
{
    _arr.assign_bytes(buffer, this->field_byte_size());
    return true;
}

bool    Message::field_read_from(const uint8_t *buffer, size_t size)
{
    return _arr.copy_from_bytes(buffer, size);
}

bool    Message::field_write_to(uint8_t *buffer, size_t size)
{
    size = std::min(size, _arr.size());
    return memcpy(buffer, _arr.buf(), size) != nullptr;
}

bool    Message::add_field(const std::string & name, IMessageField *msg)
{
    Named *named = dynamic_cast<Named *>(msg);
    if (named == nullptr)
    {
        SIHD_LOG(error, "Message: cannot add as a child: not a Named object");
        return false;
    }
    return this->add_child(name, named, true) && this->_add_field_size(msg);
}

bool    Message::add_field(const std::string & name, Type dt, size_t size)
{
    MessageField *field = this->add_child<MessageField>(name);
    bool ret = field != nullptr;
    ret = ret && field->build_array(dt, size);
    ret = ret && this->_add_field_size(field);
    return ret;
}

bool    Message::_add_field_size(IMessageField *field)
{
    _total_size += field->field_byte_size();
    return true;
}

bool    Message::finish()
{
    if (this->is_finished())
        return false;
    __assign_arr_at = 0;
    _arr.resize(_total_size);
    for (const std::string & name: this->children_keys())
    {
        IMessageField *field = this->find<IMessageField>(name);
        if (field == nullptr || this->_assign_field_array(field) == false)
            return false;
    }
    _finished = true;
    return true;
}

bool    Message::_assign_field_array(IMessageField *field)
{
    bool ret = field->assign_field_buffer(_arr.buf() + __assign_arr_at);
    if (ret)
    {
        __assign_arr_at += field->field_byte_size();
        if (__assign_arr_at > _total_size)
        {
            SIHD_LOG_ERROR("Message: for %s total size %lu is inferior to calculated size %lu",
                        this->name().c_str(), _total_size, __assign_arr_at);
            return false;
        }
    }
    return ret;
}

}