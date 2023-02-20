#include <sihd/util/Logger.hpp>
#include <sihd/util/MessageField.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/array_utils.hpp>

namespace sihd::util
{

SIHD_LOGGER;

SIHD_UTIL_REGISTER_FACTORY(MessageField);

MessageField::MessageField(const std::string & name, Node *parent): Named(name, parent)
{
    _dt = TYPE_NONE;
    _size = 0;
    _array_ptr = nullptr;
}

MessageField::~MessageField()
{
    this->_delete_array();
}

bool MessageField::build_array(Type dt, size_t size)
{
    this->_delete_array();
    _array_ptr = array_utils::create_from_type(dt);
    _dt = dt;
    _size = size;
    return _array_ptr != nullptr;
}

IMessageField *MessageField::clone() const
{
    MessageField *cloned = new MessageField(this->name());
    const IArray *arr = this->array();
    if (arr != nullptr && cloned != nullptr)
    {
        if (!cloned->build_array(arr->data_type(), arr->size()))
        {
            delete cloned;
            cloned = nullptr;
        }
    }
    return cloned;
}

bool MessageField::field_assign_buffer(void *buffer)
{
    if (_size == 0 || _dt == TYPE_NONE || _array_ptr == nullptr)
    {
        SIHD_LOG_ERROR("MessageField: for {} cannot assign array before it is built", this->name());
        return false;
    }
    return _array_ptr->assign_bytes(buffer, this->field_byte_size());
}

bool MessageField::field_read_from(const void *buffer, size_t size)
{
    return _array_ptr != nullptr && _array_ptr->copy_from_bytes(buffer, size);
}

bool MessageField::field_write_to(void *buffer, size_t size)
{
    if (_array_ptr == nullptr)
        return false;
    size = std::min(size, _array_ptr->byte_size());
    return memcpy(buffer, _array_ptr->buf(), size) != nullptr;
}

void MessageField::_delete_array()
{
    if (_array_ptr != nullptr)
        delete _array_ptr;
    _array_ptr = nullptr;
}

bool MessageField::field_resize(size_t size)
{
    if (_array_ptr == nullptr || _array_ptr->resize(size) == false)
        return false;
    _size = size;
    return true;
}

std::string MessageField::description() const
{
    return fmt::format("{}[{}]", Types::type_str(_dt), this->field_size());
}

} // namespace sihd::util
