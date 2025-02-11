#ifndef __SIHD_UTIL_IARRAY_HPP__
#define __SIHD_UTIL_IARRAY_HPP__

#include <memory>
#include <string_view>

#include <sihd/util/type.hpp>

namespace sihd::util
{

class IArrayView;

class IArray
{
    public:
        virtual ~IArray() = default;

        // informations

        virtual uint8_t *buf() = 0;
        virtual const uint8_t *buf() const = 0;
        virtual size_t byte_index(size_t index) const = 0;
        virtual uint8_t *buf_at(size_t index) = 0;
        virtual const uint8_t *buf_at(size_t index) const = 0;
        virtual size_t size() const = 0;
        virtual size_t capacity() const = 0;
        virtual size_t data_size() const = 0;
        virtual size_t byte_size() const = 0;
        virtual size_t byte_capacity() const = 0;
        virtual bool empty() const = 0;

        virtual IArray *clone_array() const = 0;

        // data type checking

        virtual bool is_same_type(const IArray & arr) const = 0;
        virtual bool is_same_type(const IArrayView & arr) const = 0;
        virtual Type data_type() const = 0;
        virtual const char *data_type_str() const = 0;

        // comparison

        virtual bool is_bytes_equal(const IArray & arr, size_t byte_offset = 0) const = 0;
        virtual bool is_bytes_equal(const IArrayView & arr, size_t byte_offset = 0) const = 0;
        virtual bool is_bytes_equal(const void *buf, size_t size, size_t byte_offset = 0) const = 0;

        // create new internal buffer

        virtual bool from_bytes(const IArray & arr) = 0;
        virtual bool from_bytes(const IArrayView & arr) = 0;
        virtual bool from_bytes(const void *buf, size_t byte_size) = 0;
        virtual bool from_str(std::string_view data, std::string_view delimiters) = 0;

        // copy data to internal buffer

        virtual bool copy_from_bytes(const IArray & arr, size_t byte_offset = 0) = 0;
        virtual bool copy_from_bytes(const IArrayView & arr, size_t byte_offset = 0) = 0;
        virtual bool copy_from_bytes(const void *buf, size_t byte_size, size_t byte_offset = 0) = 0;

        // copy to data from internal buffer

        virtual bool copy_to_bytes(void *buf, size_t byte_size, size_t byte_offset = 0) const = 0;

        // assign buffer to internal buffer

        virtual bool assign_bytes(void *buf, size_t byte_size) = 0;

        // modify capacity

        virtual bool byte_resize(size_t byte_size) = 0;
        virtual bool byte_reserve(size_t byte_capacity) = 0;

        // no byte size
        virtual bool resize(size_t size) = 0;
        // no byte size
        virtual bool reserve(size_t capacity) = 0;
        // resize to 0
        virtual void clear() = 0;

        // views

        virtual std::string hexdump(char delimiter = ' ') const = 0;
        virtual std::string str() const = 0;
        virtual std::string str(char delimiter) const = 0;

        virtual std::string cpp_str() const = 0;
        virtual std::string_view cpp_str_view() const = 0;
};

typedef std::unique_ptr<IArray> IArrayUnique;
typedef std::shared_ptr<IArray> IArrayShared;
typedef std::weak_ptr<IArray> IArrayWeak;

} // namespace sihd::util

#endif