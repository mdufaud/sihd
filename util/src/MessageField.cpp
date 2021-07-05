#include <sihd/util/MessageField.hpp>

namespace sihd::util
{

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