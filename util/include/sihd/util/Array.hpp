#ifndef __SIHD_UTIL_ARRAY_HPP__
# define __SIHD_UTIL_ARRAY_HPP__

# include <cstdint>
# include <string.h>
# include <stdexcept>
# include <memory>
# include <sihd/util/ICloneable.hpp>
# include <sihd/util/Datatype.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/IBuffer.hpp>

namespace sihd::util
{

template <typename T>
class Array:    virtual public IBuffer,
                virtual public ICloneable<Array<T>>
{
    public:
        Array()
        {
            _init();
        };

        Array(size_t capacity)
        {
            _init();
            this->reserve(capacity);
        }

        virtual ~Array()
        {
            this->delete_buffer();
        };

        Endian::Endianness  endianness;

        // IBuffer
        virtual uint8_t *buf() { return (uint8_t *)_buf_ptr; }
        virtual size_t  data_size() { return sizeof(T); }
        virtual size_t  size() { return _size; }
        virtual size_t  capacity() { return _capacity; }

        virtual void    assign(uint8_t *buf, size_t size) { this->assign(buf, size, size); }

        virtual bool    from(IBuffer & obj)
        {
            //TODO endianness
            if (obj.data_type() != this->data_type())
                return false;
            return this->from(obj.buf(), obj.capacity());
        }

        virtual Datatypes    data_type() { return Datatype::type_to_datatype<T>(); }
        virtual std::string   data_type_to_string() { return Datatype::datatype_to_string(this->data_type()); } 

        virtual Endian::Endianness  get_endianness() { return this->endianness; }

        // ICloneable

        virtual std::unique_ptr<Array<T>>    clone()
        {
            std::unique_ptr<Array<T>> ret = std::make_unique<Array<T>>();
            ret.get()->from(*this);
            return ret;
        }

        // Class methods

        T       *data() const { return _buf_ptr; }

        void    assign(void *buf, size_t size, size_t capacity)
        {
            this->delete_buffer();
            _buf_ptr = (T *)buf;
            _size = size;
            _capacity = capacity;
            _has_responsability = false;
        }

        bool    new_buffer(size_t capacity, bool clear_mem = false)
        {
            this->delete_buffer();
            _buf_ptr = new T[capacity]();
            _has_responsability = true;
            _capacity = _buf_ptr == nullptr ? 0 : capacity;
            if (_buf_ptr != nullptr && clear_mem)
                bzero(_buf_ptr, _capacity * this->data_size());
            _size = 0;
            return _buf_ptr != nullptr;
        }

        bool    resize(size_t size)
        {
            if (_buf_ptr != nullptr && size <= _capacity)
            {
                _size = size;
                return true;
            }
            return this->reserve(size) && this->resize(size);
        }

        bool    reserve(size_t capacity)
        {
            if (_buf_ptr != nullptr)
            {
                T *newbuf = new T[capacity]();
                if (newbuf == nullptr)
                    return false;
                size_t min = std::min(capacity, _size);
                memcpy((void *)newbuf, (void *)_buf_ptr, min * this->data_size());
                this->delete_buffer();
                _buf_ptr = newbuf;
                _size = min;
                _capacity = capacity;
                _has_responsability = true;
            }
            else
                this->new_buffer(capacity, true);
            return _buf_ptr != nullptr;
        }

        /**
         * @brief Creates a new array from a source
         * 
         * @param buf Array to copy from
         * @param size The size of bytes to copy from buffer
         * @return true if correctly allocated 
         */
        bool    from(uint8_t *buf, size_t size)
        {
            if (buf == nullptr)
                return false;
            if (this->new_buffer(size))
            {
                memcpy(_buf_ptr, buf, size * this->data_size());
                _size = size;
            }
            return _buf_ptr != nullptr;
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
                this->reserve(_size + std::max(Array::added_resize_capacity, size));
            if (_buf_ptr != nullptr)
            {
                memcpy((uint8_t *)(_buf_ptr + _size), buf, size * this->data_size());
                _size += size;
            }
            return _buf_ptr != nullptr;
        }

        bool    push_back(const T & value)
        {
            if (_size + 1 > _capacity)
                this->reserve(_size + Array::added_resize_capacity);
            if (_buf_ptr != nullptr)
            {
                _buf_ptr[_size] = value;
                ++_size;
            }
            return _buf_ptr != nullptr;
        }

        T    pop(size_t idx)
        {
            T ret = this->at(idx);
            size_t len = (_size - (idx + 1)) * this->data_size();
            memmove((uint8_t *)(_buf_ptr + idx),
                    (uint8_t *)(_buf_ptr + idx + 1),
                    len);
            memset((uint8_t *)(_buf_ptr + _size - 1), 0, this->data_size());
            _size -= 1;
            return ret;
        }

        T   front() const
        {
            return _buf_ptr[0];
        }

        T   back() const
        {
            return _buf_ptr[_size - 1];
        }

        T   at(size_t idx) const
        {
            if (idx >= _size)
                throw std::out_of_range("index exceeds size");
            return _buf_ptr[idx];
        }

        inline T    operator[](size_t idx) const
        {
            return _buf_ptr[idx];
        }

        inline T &  operator[](size_t idx)
        {
            return _buf_ptr[idx];
        }

        bool    fill(T value, size_t from = 0)
        {
            if (_buf_ptr != nullptr)
            {
                size_t i = from;
                while (i < _capacity)
                {
                    _buf_ptr[i] = value;
                    ++i;
                }
            }
            return _buf_ptr != nullptr;
        }

        void    delete_buffer()
        {
            if (_buf_ptr != nullptr && _has_responsability)
                delete[] _buf_ptr;
            _buf_ptr = nullptr;
            _has_responsability = false;
        }

    private:
        void    _init()
        {
            endianness = Endian::get_endian();
            _buf_ptr = nullptr;
            _size = 0;
            _capacity = 0;
            _has_responsability = false;
        }

        T       *_buf_ptr;
        size_t  _size;
        size_t  _capacity;
        bool    _has_responsability;

        static size_t   added_resize_capacity;
};

template <typename T>
size_t Array<T>::added_resize_capacity = 1;

typedef Array<bool>       Bool;
typedef Array<int8_t>     Byte;
typedef Array<uint8_t>    UByte;
typedef Array<int16_t>    Short;
typedef Array<uint16_t>   UShort;
typedef Array<int32_t>    Int;
typedef Array<uint32_t>   UInt;
typedef Array<int64_t>    Long;
typedef Array<uint64_t>   ULong;
typedef Array<float>      Float;
typedef Array<double>     Double;

}

#endif 