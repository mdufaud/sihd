#ifndef __SIHD_UTIL_IARRAY_HPP__
# define __SIHD_UTIL_IARRAY_HPP__

# include <cstdint>
# include <string.h>
# include <stdexcept>
# include <memory>
# include <sihd/util/Endian.hpp>
# include <sihd/util/ICloneable.hpp>
# include <sihd/util/Datatype.hpp>
# include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

class IArray
{
    public:
        virtual ~IArray() {};

        virtual uint8_t *buf() = 0;
        virtual const uint8_t *cbuf() const = 0;
        virtual size_t size() const = 0;
        virtual size_t capacity() const = 0;
        virtual size_t data_size() const = 0;
        virtual size_t byte_size() const = 0;
        virtual size_t byte_capacity() const = 0;

        virtual bool is_same_type(const IArray & arr) const = 0;
        virtual bool is_equal(const IArray & arr) const = 0;

        virtual bool from(const IArray & arr) = 0;
        virtual bool copy_from(const IArray & arr, size_t from = 0) = 0;
        virtual bool copy_from_bytes(const uint8_t *buf, size_t size, size_t from = 0) = 0;
        virtual bool copy_to(uint8_t *buf, size_t size) const = 0;
        virtual bool assign_bytes(uint8_t *buf, size_t size) = 0;

        virtual bool byte_resize(size_t size) = 0;
        virtual bool byte_reserve(size_t capacity) = 0;

        // no byte size
        virtual bool resize(size_t size) = 0;
        // no byte size
        virtual bool reserve(size_t capacity) = 0;

        virtual Type data_type() const = 0;
        virtual std::string data_type_to_string() const = 0;

        virtual std::string hexdump(char delimiter) const = 0;

};

}

#endif 