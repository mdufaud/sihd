#ifndef __SIHD_UTIL_BUF_HPP__
# define __SIHD_UTIL_BUF_HPP__

# include <cstdint>
# include <string.h>
# include <stdexcept>
# include <memory>
# include <sihd/util/endian.hpp>
# include <sihd/util/ICloneable.hpp>
# include <sihd/util/datatype.hpp>

namespace sihd::util
{

template <typename T>
class Buf:  public ICloneable<Buf<T>>
{
    public:
        Buf()
        {
            _init();
        }

        Buf(size_t capacity)
        {
            _init();
            this->new_buffer(capacity);
        }

        virtual ~Buf()
        {
            this->delete_buffer();
        }

        bool    new_buffer(size_t capacity, bool clear_mem = false)
        {
            this->delete_buffer();
            _buf_p = new T[capacity]();
            _has_responsability = true;
            _capacity = _buf_p == nullptr ? 0 : capacity;
            if (_buf_p != nullptr && clear_mem)
                bzero(_buf_p, _capacity * sizeof(T));
            _size = 0;
            endianness = endian::get_endian();
            return _buf_p != nullptr;
        }

        bool    resize(size_t capacity, bool ret_if_capacity = false)
        {
            if (_buf_p != nullptr)
            {
                if (ret_if_capacity == true && _capacity >= capacity)
                    return true;
                T *newbuf = new T[capacity]();
                if (newbuf == nullptr)
                    return false;
                size_t min = std::min(capacity, _size);
                memcpy((void *)newbuf, (void *)_buf_p, min * sizeof(T));
                this->delete_buffer();
                _buf_p = newbuf;
                _size = min;
                _capacity = capacity;
                _has_responsability = true;
            }
            else
                this->new_buffer(capacity, true);
            return _buf_p != nullptr;
        }

        bool    from(const Buf<T> & buf)
        {
            return this->from(buf.data(), buf.capacity(), buf.endianness);
        }

        bool    from(const T *buf, size_t size, endian::Endianness endian = endian::get_endian())
        {
            if (buf == nullptr)
                return false;
            if (this->new_buffer(size))
            {
                endianness = endian;
                memcpy(_buf_p, buf, size * sizeof(T));
                _size = size;
            }
            return _buf_p != nullptr;
        }

        void    assign(void *buf, size_t size)
        {
            this->assign(buf, size, size);
        }

        void    assign(void *buf, size_t size, size_t capacity)
        {
            this->delete_buffer();
            _buf_p = (T *)buf;
            _size = size;
            _capacity = capacity;
        }

        bool    copy(const void *buf, size_t size, size_t at = 0)
        {
            if (size + at > _capacity)
                return false;
            memcpy(this->buf() + at, buf, size);
            _size = size + at;
            return true;
        }

        bool    push_back(const T *buf, size_t size)
        {
            if (_size + size > _capacity)
                this->resize(_size + size);
            if (_buf_p != nullptr)
            {
                memcpy((uint8_t *)(_buf_p + _size), buf, size * sizeof(T));
                _size += size;
            }
            return _buf_p != nullptr;
        }

        bool    push_back(const T & value)
        {
            if (_size + 1 > _capacity)
                this->resize(_size + 1);
            if (_buf_p != nullptr)
            {
                _buf_p[_size] = value;
                ++_size;
            }
            return _buf_p != nullptr;
        }

        T    pop(size_t idx)
        {
            T ret = this->at(idx);
            memmove(_buf_p[idx], _buf_p[idx + 1], _size - (idx + 1));
            return ret;
        }

        // bool    pop(size_t idx, size_t len);

        T   at(size_t idx) const
        {
            if (idx >= _capacity)
                throw std::out_of_range("index too high");
            return _buf_p[idx];
        }

        inline T    operator[](size_t idx) const
        {
            return _buf_p[idx];
        }

        inline T &  operator[](size_t idx)
        {
            return _buf_p[idx];
        }

        bool    fill(T value, size_t from = 0)
        {
            if (_buf_p != nullptr)
            {
                size_t i = from;
                while (i < _capacity)
                {
                    _buf_p[i] = value;
                    ++i;
                }
            }
            return _buf_p != nullptr;
        }

        virtual std::unique_ptr<Buf<T>>    clone() const
        {
            std::unique_ptr<Buf<T>>    ret = std::make_unique<Buf<T>>();
            ret.get()->from(*this);
            return ret;
        }

        void    delete_buffer()
        {
            if (_buf_p != nullptr && _has_responsability)
                delete[] _buf_p;
            _has_responsability = false;
        }

        void    data_type()
        {
            return datatype::type_to_datatype<T>();
        }

        std::string     data_type_to_string()
        {
            return datatype::datatype_to_string(this->data_type());
        }

        uint8_t *buf() const { return (uint8_t *)_buf_p; }
        T       *data() const { return _buf_p; }
        size_t  size() const { return _size; }
        size_t  capacity() const { return _capacity; }
        size_t  data_size() const { return sizeof(T); }

        endian::Endianness  endianness;

    private:
        void    _init()
        {
            _buf_p = nullptr;
            _size = 0;
            _capacity = 0;
            _has_responsability = false;
            endianness = endian::get_endian();
        }

        T       *_buf_p;
        size_t  _size;
        size_t  _capacity;
        bool    _has_responsability;


};

typedef Buf<bool>       Bool;
typedef Buf<int8_t>     Byte;
typedef Buf<uint8_t>    UByte;
typedef Buf<int16_t>    Short;
typedef Buf<uint16_t>   UShort;
typedef Buf<int32_t>    Int;
typedef Buf<uint32_t>   UInt;
typedef Buf<int64_t>    Long;
typedef Buf<uint64_t>   ULong;
typedef Buf<float>      Float;
typedef Buf<double>     Double;
typedef Buf<std::string>    String;

}

#endif 