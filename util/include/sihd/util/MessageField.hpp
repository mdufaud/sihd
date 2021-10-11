#ifndef __SIHD_UTIL_MESSAGEFIELD_HPP__
# define __SIHD_UTIL_MESSAGEFIELD_HPP__

# include <sihd/util/Logger.hpp>
# include <sihd/util/Datatype.hpp>
# include <sihd/util/Array.hpp>
# include <sihd/util/Named.hpp>
# include <sihd/util/IMessageField.hpp>

namespace sihd::util
{

class MessageField: public sihd::util::Named,
                    virtual public IMessageField
{
    public:
        MessageField(const std::string & name, Node *parent = nullptr);
        ~MessageField();

        // IMessageField
        virtual size_t  get_field_byte_size() override { return _size * Datatype::datatype_size(_dt); }
        virtual bool    assign_field_buffer(uint8_t *buffer) override;
        virtual bool    field_read_from(uint8_t *buffer, size_t size) override;
        virtual bool    field_write_to(uint8_t *buffer, size_t size) override;
        virtual bool    is_finished() const override { return _array_ptr != nullptr; }
        virtual bool    finish() override { return this->is_finished(); }

        // Named
        virtual std::string get_description() const override
        {
            return Str::format("%s[%lu]", Datatype::datatype_to_string(_dt).c_str(), _size);
        }

        // ICloneable
        virtual IMessageField *clone() override;

        template <typename T>
        T   read_value(size_t idx)
        {
            if (_array_ptr == nullptr)
                throw std::logic_error("array is not built yet");
            return ArrayUtil::read_array<T>(*_array_ptr, idx);
        }
        virtual bool build_array(Datatypes dt, size_t size);
        IArray *array() { return _array_ptr; };

    protected:

    private:
        void    _delete_array();

        Datatypes   _dt;
        size_t      _size;
        IArray      *_array_ptr;
};

}

#endif 