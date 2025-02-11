#ifndef __SIHD_UTIL_IARRAYVIEW_HPP__
#define __SIHD_UTIL_IARRAYVIEW_HPP__

#include <string>

#include <sihd/util/type.hpp>

namespace sihd::util
{

class IArrayView
{
    public:
        virtual ~IArrayView() {};

        // informations

        virtual const uint8_t *buf() const = 0;
        virtual size_t byte_index(size_t index) const = 0;
        virtual const uint8_t *buf_at(size_t index) const = 0;
        virtual size_t size() const = 0;
        virtual size_t data_size() const = 0;
        virtual size_t byte_size() const = 0;
        virtual bool empty() const = 0;

        // data type checking

        virtual bool is_same_type(const IArrayView & arr) const = 0;
        virtual Type data_type() const = 0;
        virtual const char *data_type_str() const = 0;

        // comparison

        virtual bool is_bytes_equal(const IArrayView & arr, size_t byte_offset = 0) const = 0;
        virtual bool is_bytes_equal(const void *buf, size_t size, size_t byte_offset = 0) const = 0;

        // copy to data from internal buffer

        virtual bool copy_to_bytes(void *buf, size_t byte_size, size_t byte_offset = 0) const = 0;

        // views

        virtual std::string hexdump(char delimiter = ' ') const = 0;
        virtual std::string str() const = 0;
        virtual std::string str(char delimiter) const = 0;

        virtual std::string cpp_str() const = 0;
        virtual std::string_view cpp_str_view() const = 0;
};

} // namespace sihd::util

#endif