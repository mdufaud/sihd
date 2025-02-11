#ifndef __SIHD_UTIL_IMESSAGEFIELD_HPP__
#define __SIHD_UTIL_IMESSAGEFIELD_HPP__

#include <sihd/util/ICloneable.hpp>
#include <sihd/util/type.hpp>

namespace sihd::util
{

class IMessageField: public ICloneable<IMessageField>
{
    public:
        virtual ~IMessageField() = default;

        virtual Type field_type() const = 0;
        virtual size_t field_size() const = 0;
        virtual size_t field_byte_size() const = 0;
        virtual bool field_assign_buffer(void *buffer) = 0;
        virtual bool field_read_from(const void *buffer, size_t size) = 0;
        virtual bool field_write_to(void *buffer, size_t size) = 0;
        virtual bool field_resize(size_t size) = 0;
        virtual bool is_finished() const = 0;
        virtual bool finish() = 0;
};
} // namespace sihd::util

#endif