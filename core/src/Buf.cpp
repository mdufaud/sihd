#include <sihd/core/Buf.hpp>
#include <algorithm>

namespace sihd::core
{

Buf::Buf()
{
    _init();
}

// TODO THROW IF NOT ALLOC

Buf::Buf(size_t capacity)
{
    _init();
    this->new_buffer(capacity, 0);
}

Buf::Buf(const void *buf, size_t size)
{
    _init();
    this->from(buf, size);
}

Buf::Buf(const void *buf, size_t size, size_t capacity)
{
    _init();
    if (capacity < size)
    {}
    if (this->new_buffer(capacity) == false)
    {}
    this->copy(buf, size, 0);
}

Buf::~Buf()
{
    if (_buf_p != nullptr)
        delete[] _buf_p;
}

void    Buf::_init()
{
    _buf_p = nullptr;
    _size = 0;
    _capacity = 0;
    endianness = endian::get_endian();
}

bool    Buf::resize(size_t capacity)
{
    if (_buf_p != nullptr)
    {
        uint8_t *newbuf = new uint8_t[capacity]();
        if (newbuf == nullptr)
            return false;
        size_t min = std::min(capacity, _size);
        memcpy(newbuf, _buf_p, min);
        delete[] _buf_p;
        _buf_p = newbuf;
        _size = min;
        _capacity = capacity;
    }
    else
        this->new_buffer(capacity, 0);
    return _buf_p != nullptr;
}

bool    Buf::new_buffer(size_t capacity)
{
    if (_buf_p != nullptr)
        delete[] _buf_p;
    _buf_p = new uint8_t[capacity]();
    _capacity = _buf_p == nullptr ? 0 : capacity;
    _size = 0;
    endianness = endian::get_endian();
    return _buf_p != nullptr;
}

bool    Buf::new_buffer(size_t capacity, char fill)
{
    if (this->new_buffer(capacity))
        memset(_buf_p, fill, capacity);
    return _buf_p != nullptr;
}

bool    Buf::from(const Buf & buf)
{
    return this->from(buf.buf(), buf.capacity(), buf.endianness);
}

bool    Buf::from(const char *str, endian::Endianness endian)
{
    return str != nullptr ? this->from(str, strlen(str), endian) : false;
}

bool    Buf::from(const void *buf, size_t size, endian::Endianness endian)
{
    if (buf == nullptr)
        return false;
    if (this->new_buffer(size))
    {
        endianness = endian;
        memcpy(_buf_p, buf, size);
        _size = size;
    }
    return _buf_p != nullptr;
}

bool    Buf::append(const void *buf, size_t size)
{
    if (size + _size > _capacity)
        this->resize(size + _size);
    return _buf_p != nullptr ? this->copy(buf, size, _size) : false;
}

// bool    Buf::remove(size_t idx, size_t len)
// {
//     //TODO USE memmove
// }

void    Buf::assign(void *buf, size_t size)
{
    this->assign(buf, size, size);
}

void    Buf::assign(void *buf, size_t size, size_t capacity)
{
    if (_buf_p != nullptr)
        delete[] _buf_p;
    _buf_p = (uint8_t *)buf;
    _size = size;
    _capacity = capacity;
}

bool    Buf::fill(char c)
{
    if (_buf_p)
        memset(_buf_p, c, _capacity);
    return _buf_p != nullptr;
}

uint8_t    Buf::at(size_t idx) const
{
    if (idx >= _capacity)
        throw std::out_of_range("index too high");
    return _buf_p[idx];
}

bool    Buf::copy(const void *buf, size_t size, size_t at)
{
    if (size + at > _capacity)
        return false;
    memcpy(_buf_p + at, buf, size);
    _size = size + at;
    return true;
}

bool    Buf::write(size_t idx, uint8_t value)
{
    if (idx >= _capacity)
        return false;
    _buf_p[idx] = value;
    return true;
}

}