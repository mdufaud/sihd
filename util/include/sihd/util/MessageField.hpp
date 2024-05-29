#ifndef __SIHD_UTIL_MESSAGEFIELD_HPP__
#define __SIHD_UTIL_MESSAGEFIELD_HPP__

#include <sihd/util/IMessageField.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Types.hpp>
#include <sihd/util/array_utils.hpp>

namespace sihd::util
{

class MessageField: public Named,
                    public IMessageField
{
    public:
        MessageField(const std::string & name, Node *parent = nullptr);
        ~MessageField();

        // IMessageField
        virtual size_t field_size() const override { return _size; }
        virtual size_t field_byte_size() const override { return _size * Types::type_size(_dt); }
        virtual Type field_type() const override { return _dt; }
        virtual bool field_assign_buffer(void *buffer) override;
        virtual bool field_read_from(const void *buffer, size_t size) override;
        virtual bool field_write_to(void *buffer, size_t size) override;
        virtual bool field_resize(size_t size) override;
        virtual bool is_finished() const override { return _array_ptr != nullptr; }
        virtual bool finish() override { return this->is_finished(); }

        virtual bool build_array(Type dt, size_t size);

        // Named
        virtual std::string description() const override;

        // ICloneable
        virtual IMessageField *clone() const override;

        template <typename T>
        T read_value(size_t idx) const
        {
            if (_array_ptr == nullptr)
                throw std::logic_error("array is not built yet");
            return array_utils::read<T>(*_array_ptr, idx);
        }

        const IArray *array() const { return _array_ptr; }

    protected:

    private:
        void _delete_array();

        Type _dt;
        size_t _size;
        IArray *_array_ptr;
};

} // namespace sihd::util

#endif
