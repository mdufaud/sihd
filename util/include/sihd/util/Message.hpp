#ifndef __SIHD_UTIL_MESSAGE_HPP__
# define __SIHD_UTIL_MESSAGE_HPP__

# include <list>
# include <string>
# include <stdexcept>
# include <sihd/util/Datatype.hpp>
# include <sihd/util/Array.hpp>
# include <sihd/util/OrderedNode.hpp>
# include <sihd/util/Logger.hpp>

namespace sihd::util
{

class IMessageField:    virtual public ICloneable<IMessageField>
{
    public:
        virtual ~IMessageField() {};
        virtual size_t  get_field_byte_size() = 0;
        virtual bool    assign_field_buffer(uint8_t *buffer) = 0;
        virtual bool    field_read_from(uint8_t *buffer, size_t size) = 0;
        virtual bool    field_write_to(uint8_t *buffer, size_t size) = 0;
        virtual bool    is_finished() const = 0;
        virtual bool    finish() = 0;
};

class MessageField: public sihd::util::Named,
                    virtual public IMessageField
{
    public:
        MessageField(const std::string & name, Node *parent = nullptr);
        ~MessageField();

        // IMessageField
        virtual size_t  get_field_byte_size() { return _size * Datatype::datatype_size(_dt); }
        virtual bool    assign_field_buffer(uint8_t *buffer);
        virtual bool    field_read_from(uint8_t *buffer, size_t size);
        virtual bool    field_write_to(uint8_t *buffer, size_t size);
        virtual bool    is_finished() const { return _array_ptr != nullptr; }
        virtual bool    finish() { return this->is_finished(); }

        // Named
        virtual std::string     get_description() const override
        {
            return Str::format("%s[%lu]", Datatype::datatype_to_string(_dt).c_str(), _size);
        }

        // ICloneable
        virtual IMessageField *clone();

        template <typename T>
        T   read_value(size_t idx)
        {
            if (_array_ptr == nullptr)
                throw std::logic_error("array is not built yet");
            return ArrayUtil::read_array<T>(_array_ptr, idx);
        }
        virtual bool    build_array(Datatypes dt, size_t size);
        IArray  *array() { return _array_ptr; };

    protected:

    private:
        void    _delete_array();

        Datatypes   _dt;
        size_t      _size;
        IArray      *_array_ptr;
};

class Message:  public sihd::util::OrderedNode,
                virtual public IMessageField
{
    public:
        Message(const std::string & name, Node *parent = nullptr);
        virtual ~Message();

        // IMessageField
        virtual size_t  get_field_byte_size() { return _total_size; }
        virtual bool    assign_field_buffer(uint8_t *buffer);
        virtual bool    field_read_from(uint8_t *buffer, size_t size);
        virtual bool    field_write_to(uint8_t *buffer, size_t size);
        virtual bool    is_finished() const { return _finished; }
        virtual bool    finish();

        // ICloneable
        virtual IMessageField *clone();

        template <typename T>
        bool    add_field(const std::string & name, size_t size = 1)
        {
            return this->add_field(name, Datatype::type_to_datatype<T>(), size);
        }
        virtual bool    add_field(const std::string & name, Datatypes dt, size_t size = 1);
        /**
         * @brief 
         * 
         * @param name 
         * @param field must be a sihd::util::Named to be added as a composite
         * @return true if added
         * @return false if not a Named
         */
        virtual bool    add_field(const std::string & name, IMessageField *field);

        // Named
        virtual std::string     get_description() const override
        {
            return Str::format("%lu bytes", _total_size);
        }

        IArray  *array() { return &_arr; };

    protected:
        virtual bool    _add_field_size(IMessageField *field);
        virtual bool    _assign_field_array(IMessageField *field);

    private:
        Byte    _arr;
        size_t  _total_size;
        bool    _finished;
        size_t  __assign_arr_at;
};

}

#endif 