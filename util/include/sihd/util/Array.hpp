#ifndef __SIHD_UTIL_ARRAY_HPP__
# define __SIHD_UTIL_ARRAY_HPP__

# include <cstdint>
# include <strings.h>
# include <stdexcept>
# include <memory>
# include <sihd/util/ICloneable.hpp>
# include <sihd/util/Datatype.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/IArray.hpp>

namespace sihd::util
{

LOGGER;

template <typename T>
class Array:    virtual public IArray,
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
            if (capacity > 0)
                this->reserve(capacity);
        }

        Array(std::initializer_list<T> list)
        {
            _init();
            this->reserve(list.size());
            for (const T & value: list)
            {
                this->push_back(value);
            }
        }

        virtual ~Array()
        {
            this->delete_buffer();
        };

        // IArray
        virtual uint8_t *buf() { return (uint8_t *)_buf_ptr; }
        virtual size_t  data_size() const { return sizeof(T); }
        virtual size_t  size() const { return _size; }
        virtual size_t  byte_size() const { return _size * sizeof(T); }
        virtual size_t  capacity() const { return _capacity; }
        virtual size_t  byte_capacity() const { return _capacity * sizeof(T); }
        virtual bool    copy_from(IArray *obj, size_t from = 0)
        {
            if (this->is_same_type(obj) == false)
                return false;
            return this->copy_from_bytes(obj->buf(), obj->byte_size(), from); 
        }
        virtual bool    copy_from_bytes(const uint8_t *buf, size_t size, size_t from = 0)
        {
            if (size + from > this->byte_capacity())
                return false;
            memcpy(this->buf() + from, buf, size);
            return true;
        }
        virtual bool    copy_to(uint8_t *buf, size_t size) const
        {
            if (size > this->byte_capacity())
                return false;
            memcpy(buf, (void *)_buf_ptr, size);
            return true;
        }
        virtual bool    from(IArray *obj)
        {
            if (this->is_same_type(obj) == false)
                return false;
            return this->from(obj->buf(), obj->capacity());
        }
        virtual bool    assign_bytes(uint8_t *buf, size_t size)
        {
            return this->assign_bytes(buf, size, size);
        }

        virtual Datatypes   data_type() const { return Datatype::type_to_datatype<T>(); }
        virtual std::string data_type_to_string() const { return Datatype::datatype_to_string(this->data_type()); } 

        virtual bool    resize(size_t size)
        {
            if (_buf_ptr != nullptr && size <= _capacity)
            {
                _size = size;
                return true;
            }
            return this->reserve(size) && this->resize(size);
        }

        virtual bool    reserve(size_t capacity)
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

        virtual bool    is_same_type(IArray *arr)
        {
            return this->data_type() == arr->data_type();
        }

        virtual bool    is_equal(IArray *arr)
        {
            if (this->size() != arr->size() || this->is_same_type(arr) == false)
                return false;
            return memcmp(_buf_ptr, arr->buf(), this->byte_size()) == 0;
        }

        // ICloneable

        virtual Array<T>    *clone()
        {
            Array<T> *cloned = new Array<T>();
            if (cloned != nullptr)
                cloned->from(this);
            return cloned;
        }

        // Class methods

        T       *data() { return _buf_ptr; }

        bool    copy_from(T *buf, size_t size, size_t from = 0)
        {
            return this->copy_from_bytes((uint8_t *)buf, size * this->data_size(), from * this->data_size());
        }

        bool    assign_bytes(void *buf, size_t size, size_t capacity)
        {
            if (size % this->data_size() != 0)
            {
                LOG_ERROR("Array::assign_bytes cannot assign buffer - size %lu not divisible by %lu",
                            size, this->data_size());
                return false;
            }
            if (capacity % this->data_size() != 0)
            {
                LOG_ERROR("Array::assign_bytes cannot assign buffer - capacity %lu not divisible by %lu",
                            capacity, this->data_size());
                return false;
            }
            return this->assign((T *)buf, size / this->data_size(), capacity / this->data_size());
        }

        bool    assign(T *buf, size_t size)
        {
            return this->assign(buf, size, size);            
        }

        bool    assign(T *buf, size_t size, size_t capacity)
        {
            this->delete_buffer();
            _buf_ptr = buf;
            _size = size;
            _capacity = capacity;
            _has_responsability = false;
            return true;
        }

        bool    new_buffer(size_t capacity, bool clear_mem = false)
        {
            this->delete_buffer();
            _buf_ptr = new T[capacity]();
            _has_responsability = true;
            _capacity = _buf_ptr == nullptr ? 0 : capacity;
            if (_buf_ptr != nullptr && clear_mem)
                memset(_buf_ptr, 0, _capacity * this->data_size());
            _size = 0;
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
                throw std::out_of_range("Array::at: index exceeds size");
            return _buf_ptr[idx];
        }

        void    set(size_t idx, T value)
        {
            if (idx >= _size)
                throw std::out_of_range("Array::set: index exceeds size");
            _buf_ptr[idx] = value;
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

typedef Array<bool>       ArrBool;
typedef Array<char>       ArrChar;
typedef Array<int8_t>     ArrByte;
typedef Array<uint8_t>    ArrUByte;
typedef Array<int16_t>    ArrShort;
typedef Array<uint16_t>   ArrUShort;
typedef Array<int32_t>    ArrInt;
typedef Array<uint32_t>   ArrUInt;
typedef Array<int64_t>    ArrLong;
typedef Array<uint64_t>   ArrULong;
typedef Array<float>      ArrFloat;
typedef Array<double>     ArrDouble;

class ArrayUtil
{
    public:
        static IArray  *create_from_type(Datatypes dt, size_t size = 0)
        {
            switch (dt)
            {
                case DBOOL:
                    return new ArrBool(size);
                case DCHAR:
                    return new ArrChar(size);
                case DBYTE:
                    return new ArrByte(size);
                case DUBYTE:
                    return new ArrUByte(size);
                case DSHORT:
                    return new ArrShort(size);
                case DUSHORT:
                    return new ArrUShort(size);
                case DINT:
                    return new ArrInt(size);
                case DUINT:
                    return new ArrUInt(size);
                case DLONG:
                    return new ArrLong(size);
                case DULONG:
                    return new ArrULong(size);
                case DFLOAT:
                    return new ArrFloat(size);
                case DDOUBLE:
                    return new ArrDouble(size);
                default:
                    break ;
            }
            return nullptr;
        }

        template <typename T>
        static Array<T> *cast_array(IArray *ptr)
        {
            return dynamic_cast<Array<T> *>(ptr);
        }

        template <typename T>
        static T    read_array(IArray *ptr, size_t idx)
        {
            Array<T> *arr = ArrayUtil::cast_array<T>(ptr);
            if (arr == nullptr)
            {
                throw std::invalid_argument(
                    Str::format("ArrayUtil::read_array wrong type - invalid cast: %s != %s",
                                ptr->data_type_to_string().c_str(),
                                Datatype::datatype_to_string(Datatype::type_to_datatype<T>()).c_str())
                );
            }
            return arr->at(idx);
        }

        template <typename T>
        static bool    write_array(IArray *ptr, size_t idx, T value)
        {
            if (idx >= ptr->capacity())
                return false;
            Array<T> *arr = ArrayUtil::cast_array<T>(ptr);
            if (arr == nullptr)
            {
                throw std::invalid_argument(
                    Str::format("ArrayUtil::write_array wrong type - invalid cast: %s != %s",
                                ptr->data_type_to_string().c_str(),
                                Datatype::datatype_to_string(Datatype::type_to_datatype<T>()).c_str())
                );
            }
            arr->set(idx, value);
            return true;
        }

    private:
        ArrayUtil() {};
        ~ArrayUtil() {};
};

}

#endif 