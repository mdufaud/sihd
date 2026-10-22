#ifndef __SIHD_UTIL_IARRAY_HPP__
#define __SIHD_UTIL_IARRAY_HPP__

#include <memory>
#include <string_view>

#include <sihd/util/Slice.hpp>
#include <sihd/util/type.hpp>

namespace sihd::util
{

class IArrayView;

class IArray
{
    public:
        virtual ~IArray() = default;

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

        virtual bool is_same_type(const IArray & arr) const = 0;
        virtual bool is_same_type(const IArrayView & arr) const = 0;
        virtual Type data_type() const = 0;
        virtual const char *data_type_str() const = 0;

        virtual bool is_bytes_equal(const IArray & arr, Slice byte_slice = {}) const = 0;
        virtual bool is_bytes_equal(const IArrayView & arr, Slice byte_slice = {}) const = 0;
        virtual bool is_bytes_equal(const void *buf, size_t buf_byte_size, Slice byte_slice = {}) const = 0;

        virtual bool from_bytes(const IArray & arr) = 0;
        virtual bool from_bytes(const IArrayView & arr) = 0;
        virtual bool from_bytes(const void *buf, size_t buf_byte_size) = 0;
        virtual bool from_str(std::string_view data, std::string_view delimiters) = 0;

        virtual bool copy_from_bytes(const IArray & arr, Slice byte_slice = {}) = 0;
        virtual bool copy_from_bytes(const IArrayView & arr, Slice byte_slice = {}) = 0;
        virtual bool copy_from_bytes(const void *buf, size_t buf_byte_size, Slice byte_slice = {}) = 0;

        virtual bool copy_to_bytes(void *buf, Slice byte_slice = {}) const = 0;

        virtual bool assign_bytes(void *buf, size_t buf_byte_size) = 0;

        virtual bool byte_resize(size_t byte_size) = 0;
        virtual bool byte_reserve(size_t byte_capacity) = 0;

        virtual bool resize(size_t size) = 0;
        virtual bool reserve(size_t capacity) = 0;
        virtual void clear() = 0;

        virtual std::string hexdump(char delimiter = ' ') const = 0;
        virtual std::string str() const = 0;
        virtual std::string str(char delimiter) const = 0;

        virtual std::string cpp_str() const = 0;
        virtual std::string_view cpp_str_view() const = 0;
};

using IArrayUnique = std::unique_ptr<IArray>;
using IArrayShared = std::shared_ptr<IArray>;
using IArrayWeak = std::weak_ptr<IArray>;

} // namespace sihd::util

#endif