#ifndef __SIHD_UTIL_IMESSAGEFIELD_HPP__
# define __SIHD_UTIL_IMESSAGEFIELD_HPP__

# include <sihd/util/ICloneable.hpp>
# include <sihd/util/IArray.hpp>

namespace sihd::util
{

class IMessageField: public ICloneable<IMessageField>
{
    public:
        virtual ~IMessageField() {};
        virtual size_t get_field_byte_size() = 0;
        virtual bool assign_field_buffer(uint8_t *buffer) = 0;
        virtual bool field_read_from(const uint8_t *buffer, size_t size) = 0;
        virtual bool field_write_to(uint8_t *buffer, size_t size) = 0;
        virtual bool is_finished() const = 0;
        virtual bool finish() = 0;
};
}

#endif 