#include <sihd/util/Message.hpp>

namespace sihd::util
{

LOGGER;

Message::Message(const std::string & name, Node *parent): OrderedNode(name, parent) 
{
    _total_size = 0;
    _finished = false;
}

Message::~Message()
{
}

IMessageField *Message::clone()
{
    bool error = false;
    Message *cloned = new Message(this->get_name());
    for (const std::string & name: this->get_children_keys())
    {
        IMessageField *field = this->find<IMessageField>(name);
        if (field != nullptr)
        {
            IMessageField *cloned_field = field->clone();
            if (cloned_field != nullptr)
                cloned->add_field(name, cloned_field);
            else
            {
                LOG(critical, "Message: clone failed for " << name);
                error = true;
            }
        }
        else
        {
            LOG(critical, "Message: could not clone field " << name << " - not a IMessageField");
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
    _arr.assign_bytes(buffer, this->get_field_byte_size());
    return true;
}

bool    Message::field_read_from(uint8_t *buffer, size_t size)
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
        LOG(error, "Message: cannot add as a child: not a Named object");
        return false;
    }
    return this->add_child(name, named) && this->_add_field_size(msg);
}

bool    Message::add_field(const std::string & name, Datatypes dt, size_t size)
{
    MessageField *field = this->add_child<MessageField>(name);
    bool ret = field != nullptr;
    ret = ret && field->build_array(dt, size);
    ret = ret && this->_add_field_size(field);
    return ret;
}

bool    Message::_add_field_size(IMessageField *field)
{
    _total_size += field->get_field_byte_size();
    return true;
}

bool    Message::finish()
{
    if (this->is_finished())
        return false;
    __assign_arr_at = 0;
    _arr.new_buffer(_total_size, true);
    for (const std::string & name: this->get_children_keys())
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
        __assign_arr_at += field->get_field_byte_size();
        if (__assign_arr_at > _total_size)
        {
            LOG_ERROR("Message: for %s total size %lu is inferior to calculated size %lu",
                        this->get_name().c_str(), _total_size, __assign_arr_at);
            return false;
        }
    }
    return ret;
}

MessageField::MessageField(const std::string & name, Node *parent): Named(name, parent)
{
    _dt = NONE;
    _size = 0;
    _array_ptr = nullptr;
}

MessageField::~MessageField()
{
    this->_delete_array();
}

bool    MessageField::build_array(Datatypes dt, size_t size)
{
    this->_delete_array();
    _array_ptr = ArrayUtil::create_from_type(dt);
    _dt = dt;
    _size = size;
    return _array_ptr != nullptr;
}

IMessageField   *MessageField::clone()
{
    MessageField *cloned = new MessageField(this->get_name());
    IArray *arr = this->array();
    if (arr != nullptr)
        cloned->build_array(arr->data_type(), arr->size());
    return cloned;
}

bool    MessageField::assign_field_buffer(uint8_t *buffer)
{
    if (_size == 0 || _dt == NONE || _array_ptr == nullptr)
    {
        LOG_ERROR("MessageField: for %s cannot assign array before it is built",
                    this->get_name().c_str());
        return false;
    }
    return _array_ptr->assign_bytes(buffer, this->get_field_byte_size());
}

bool    MessageField::field_read_from(uint8_t *buffer, size_t size)
{
    return _array_ptr != nullptr && _array_ptr->copy_from_bytes(buffer, size);
}

bool    MessageField::field_write_to(uint8_t *buffer, size_t size)
{
    if (_array_ptr == nullptr)
        return false;
    size = std::min(size, _array_ptr->size() * _array_ptr->data_size());
    return memcpy(buffer, _array_ptr->buf(), size) != nullptr;
}

void    MessageField::_delete_array()
{
    if (_array_ptr != nullptr)
        delete _array_ptr;
    _array_ptr = nullptr;
}

}