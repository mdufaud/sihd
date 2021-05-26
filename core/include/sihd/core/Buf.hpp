#ifndef __SIHD_CORE_BUF_HPP__
# define __SIHD_CORE_BUF_HPP__

# include <cstdint>
# include <string.h>
# include <sihd/core/endian.hpp>
# include <stdexcept>
# include <memory>

namespace sihd::core
{

class Buf
{
    public:
        Buf();
        Buf(size_t capacity);
        Buf(const void *buf, size_t size);
        Buf(const void *buf, size_t size, size_t capacity);
        virtual ~Buf();

        bool    new_buffer(size_t capacity);
        bool    new_buffer(size_t capacity, char fill);
        bool    resize(size_t capacity);

        bool    from(const Buf & buf);
        // null terminated
        bool    from(const char *str, endian::Endianness endian = endian::get_endian());
        bool    from(const void *buf, size_t size, endian::Endianness endian = endian::get_endian());

        void    assign(void *buf, size_t size);
        void    assign(void *buf, size_t size, size_t capacity);

        bool    copy(const void *buf, size_t size, size_t at = 0);
        bool    append(const void *buf, size_t size);
        // bool    remove(size_t idx, size_t len);
        bool    fill(char value);

        // std::unique_ptr<Buf>    clone();

        bool    write(size_t idx, uint8_t value);

        // getters
        uint8_t at(size_t idx) const;
        uint8_t     operator[](size_t idx) const
        {
            return _buf_p[idx];
        }

        uint8_t *buf() const { return _buf_p; };
        size_t  size() const { return _size; };
        size_t  capacity() const { return _capacity; };

        endian::Endianness  endianness;

    private:
        void    _init();

        uint8_t *_buf_p;
        size_t  _size;
        size_t  _capacity;
};

}

#endif 