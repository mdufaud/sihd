#ifndef __SIHD_UTIL_ARRAY_HPP__
# define __SIHD_UTIL_ARRAY_HPP__

# include <sihd/util/Logger.hpp>
# include <sihd/util/IArray.hpp>
# include <sihd/util/Splitter.hpp>

# include <utility>
# include <cstdint>
# include <stdexcept>
# include <memory>

# include <string.h>
# include <strings.h>

namespace sihd::util
{

LOGGER;

template <typename T>
class Array: public IArray, public ICloneable<Array<T>>
{
    public:
        Array() { _init(); };

        Array(const T *data, size_t size)
        {
            _init();
            this->reserve(size);
            this->push_back(data, size);
        }

        Array(const Array<T> & array)
        {
            _init();
            this->from(array);
        }

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
                this->push_back(value);
        }

        Array & operator=(const Array<T> &) = delete;

        virtual ~Array()
        {
            this->delete_buffer();
        };

        // IArray
        uint8_t *buf() { return (uint8_t *)_buf_ptr; }
        const uint8_t *cbuf() const { return (uint8_t *)_buf_ptr; }
        size_t data_size() const { return sizeof(T); }
        size_t size() const { return _size; }
        size_t byte_size() const { return _size * sizeof(T); }
        size_t capacity() const { return _capacity; }
        size_t byte_capacity() const { return _capacity * sizeof(T); }
        Type data_type() const { return Datatype::type_to_datatype<T>(); }
        std::string data_type_to_string() const { return Datatype::datatype_to_string(this->data_type()); }
        
        bool copy_from(const IArray & arr, size_t from = 0)
        {
            if (this->is_same_type(arr))
                return this->copy_from_bytes(arr.cbuf(), arr.byte_size(), from);
            return false;
        }

        bool copy_from_bytes(const IArray & arr, size_t from = 0)
        {
            return this->copy_from_bytes(arr.cbuf(), arr.byte_size(), from); 
        }

        bool copy_from_bytes(const uint8_t *buf, size_t size, size_t from = 0)
        {
            if (size + from > this->byte_capacity())
                return false;
            memcpy(this->buf() + from, buf, size);
            return true;
        }

        bool copy_to(uint8_t *buf, size_t size) const
        {
            if (size > this->byte_capacity())
                return false;
            memcpy(buf, (void *)_buf_ptr, size);
            return true;
        }

        bool from(const IArray & arr)
        {
            if (this->is_same_type(arr) == false)
                return false;
            return this->from(arr.cbuf(), arr.capacity());
        }

        bool from_string(const std::string & data, const char *delimiters)
        {
            bool ret = true;
            Splitter splitter(delimiters);
            const std::vector<std::string> splits = splitter.split(data);
            this->reserve(splits.size());
            this->resize(0);
            for (const std::string & split: splits)
            {
                T value;
                if (Str::convert_from_string<T>(split, value))
                    this->push_back(value);
                else
                {
                    ret = false;
                    break ;
                }
            }
            return ret;
        }

        bool assign_bytes(uint8_t *buf, size_t size)
        {
            return this->assign_bytes(buf, size, size);
        }

        bool byte_resize(size_t size)
        {
            if (size % this->data_size() != 0)
            {
                LOG_ERROR("Array::byte_resize cannot resize - %lu not divisible by data size %lu",
                            size, this->data_size());
                return false;
            }
            return this->resize(size / this->data_size());
        }

        bool byte_reserve(size_t size)
        {
            if (size % this->data_size() != 0)
            {
                LOG_ERROR("Array::byte_reserve cannot reserve - %lu not divisible by data size %lu",
                            size, this->data_size());
                return false;
            }
            return this->reserve(size / this->data_size());
        }

        bool resize(size_t size)
        {
            if (_buf_ptr == nullptr || size > _capacity)
                this->reserve(size);
            if (_buf_ptr != nullptr && size <= _capacity)
                _size = size;
            return _size == size;
        }

        bool reserve(size_t capacity)
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
                _has_ownership = true;
            }
            else
                this->new_buffer(capacity, true);
            return _buf_ptr != nullptr;
        }

        bool is_same_type(const IArray & arr) const
        {
            return this->data_type() == arr.data_type();
        }

        bool is_equal(const IArray & arr) const
        {
            if (arr.byte_size() != this->byte_size())
                return false;
            return memcmp(_buf_ptr, arr.cbuf(), this->byte_size()) == 0;
        }

        std::string hexdump(char delimiter = ' ') const
        {
            return Str::hexdump(_buf_ptr, this->byte_size(), delimiter);
        }

        std::string to_string(char delimiter = '\0') const
        {
            std::stringstream ss;

            size_t i = 0;
            while (i < _size)
            {
                if (i != 0 && delimiter != '\0')
                    ss << delimiter;
                ss << std::to_string(this->at(i));
                ++i;
            }
            return ss.str();
        }

        void clear()
        {
            this->resize(0);
        }

        // ICloneable

        Array<T> *clone()
        {
            Array<T> *cloned = new Array<T>();
            if (cloned != nullptr)
                cloned->from(*this);
            return cloned;
        }

        // Class methods

        T *data() { return _buf_ptr; }
        const T *cdata() const { return _buf_ptr; }

        // compares memory from internal buffer and array of size
        bool is_equal(const T *arr, size_t size) const
        {
            if (size > this->capacity())
                return false;
            return memcmp(_buf_ptr, arr, size) == 0;
        }

        // copies values from array
        bool copy_from(const T *arr, size_t size, size_t from = 0)
        {
            return this->copy_from_bytes((uint8_t *)arr, size * this->data_size(), from * this->data_size());
        }

        // delete internal buffer if exists then sets it to bytes buffer buf - does not take ownership
        bool assign_bytes(void *buf, size_t size, size_t capacity)
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

        // delete internal buffer if exists then sets it to array buf - does not take ownership
        bool assign(T *arr, size_t size)
        {
            return this->assign(arr, size, size);            
        }

        // delete internal buffer if exists then sets it to array buf - does not take ownership
        bool assign(T *arr, size_t size, size_t capacity)
        {
            this->delete_buffer();
            _buf_ptr = arr;
            _size = size;
            _capacity = capacity;
            _has_ownership = false;
            return true;
        }

        // delete internal buffer if exists then allocates new one
        bool new_buffer(size_t capacity, bool clear_mem = false)
        {
            this->delete_buffer();
            _buf_ptr = new T[capacity]();
            _has_ownership = true;
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
         * @return true buffer allocated
         * @return false buffer not allocated
         */
        bool from(const uint8_t *buf, size_t size)
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

        // push whole array buf of size at the end of the internal buffer
        bool push_back(const T *buf, size_t size)
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

        // push a single value at the end of the array
        bool push_back(const T & value)
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

        // remove value at idx and returns it
        T pop(size_t idx)
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

        // data first value
        inline T front() const { return _buf_ptr[0]; }

        // access last value
        inline T back() const { return _buf_ptr[_size - 1]; }

        // access value at idx - throws out_of_range
        T at(size_t idx) const
        {
            if (idx >= _size)
                throw std::out_of_range("Array::at: index exceeds size");
            return _buf_ptr[idx];
        }

        // set value at idx - throws out_of_range
        void set(size_t idx, T value)
        {
            if (idx >= _size)
                throw std::out_of_range("Array::set: index exceeds size");
            _buf_ptr[idx] = value;
        }

        // access value arr[idx]
        inline T operator[](size_t idx) const { return _buf_ptr[idx]; }

        // set value arr[idx] = value
        inline T & operator[](size_t idx) { return _buf_ptr[idx]; }

        // delete internal buffer if it has ownership - set internal buffer to nullptr
        void delete_buffer()
        {
            if (_buf_ptr != nullptr && _has_ownership)
                delete[] _buf_ptr;
            _size = 0;
            _capacity = 0;
            _buf_ptr = nullptr;
            _has_ownership = false;
        }

        // Array<T> iterator

        template <typename ITERATOR_TYPE>
        class ArrayIterator
        {
            public:
                // Iterator traits - typedefs and types required to be STL compliant
                using iterator_category = std::random_access_iterator_tag;
                using difference_type = std::ptrdiff_t;
                using value_type = ITERATOR_TYPE;
                using pointer = ITERATOR_TYPE *;
                using reference = ITERATOR_TYPE &;

                pointer array_beg;
                pointer array_curr;
                pointer array_end;

                ArrayIterator(pointer ptr_begin = nullptr, pointer ptr_curr = nullptr, pointer ptr_end = nullptr):
                    array_beg(ptr_begin), array_curr(ptr_curr), array_end(ptr_end) {}

                ArrayIterator(const ArrayIterator & other):
                    array_beg(other.array_beg), array_curr(other.array_curr), array_end(other.array_end) {}

                ArrayIterator & operator=(const ArrayIterator & other)
                {
                    this->array_beg = other.array_beg;
                    this->array_curr = other.array_curr;
                    this->array_end = other.array_end;
                    return *this;
                }

                ArrayIterator & operator++()
                {
                    ++this->array_curr;
                    return *this;
                }

                ArrayIterator & operator--()
                {
                    --this->array_curr;
                    return *this;
                }

                ArrayIterator operator++(int)
                {
                    ArrayIterator ret(this);
                    ++(*this);
                    return ret;
                }

                ArrayIterator operator--(int)
                {
                    ArrayIterator ret(this);
                    --(*this);
                    return ret;
                }

                ArrayIterator & operator+=(const std::ptrdiff_t ptr_diff)
                {
                    this->array_curr += ptr_diff;
                    return *this;
                }

                ArrayIterator & operator-=(const std::ptrdiff_t ptr_diff)
                {
                    this->array_curr -= ptr_diff;
                    return *this;
                }

                difference_type operator-(const ArrayIterator & rhs) const { return this->array_curr - rhs.array_curr; }

                ArrayIterator operator+(ssize_t i)
                {
                    return ArrayIterator(this->array_beg, this->array_curr + i, this->array_end);
                }

                ArrayIterator operator-(ssize_t i)
                {
                    return ArrayIterator(this->array_beg, this->array_curr - i, this->array_end);
                }

                bool operator==(const ArrayIterator & rhs) const { return this->array_curr == rhs.array_curr; }
                bool operator!=(const ArrayIterator & rhs) const { return !(*this == rhs); }

                bool operator<(const ArrayIterator & rhs) const { return this->array_curr < rhs.array_curr; }
                bool operator<=(const ArrayIterator & rhs) const { return this->array_curr <= rhs.array_curr; }
                bool operator>(const ArrayIterator & rhs) const { return !(*this <= rhs); }
                bool operator>=(const ArrayIterator & rhs) const { return !(*this < rhs); }

                reference operator*()
                {
                    this->_check_pos(this->array_curr);
                    return *this->array_curr;
                }

            private:
                void _check_range(ssize_t movement) const { return this->_check_pos(this->array_curr + movement); }
                void _check_pos(pointer pos) const
                {
                    if (pos < this->array_beg && pos > this->array_end)
                        throw std::out_of_range("Array::iterator: iterator out of range");
                }
        };

        typedef ArrayIterator<T> iterator;
        typedef ArrayIterator<const T> const_iterator;

        iterator begin() { return iterator(this->data(), this->data(), this->data() + this->size()); }
        iterator end() { return iterator(this->data(), this->data() + this->size(), this->data() + this->size()); }

        const_iterator cbegin() { return const_iterator(this->data(), this->data(), this->data() + this->size()); }
        const_iterator cend() { return const_iterator(this->data(), this->data() + this->size(), this->data() + this->size()); }

        // end of Array<T> iterator

        // Array<T> reverse iterator

        template <typename ITERATOR_TYPE>
        class ReverseArrayIterator
        {
            public:
                // Iterator traits - typedefs and types required to be STL compliant
                using iterator_category = std::random_access_iterator_tag;
                using difference_type = std::ptrdiff_t;
                using value_type = ITERATOR_TYPE;
                using pointer = ITERATOR_TYPE *;
                using reference = ITERATOR_TYPE &;

                pointer array_beg;
                pointer array_curr;
                pointer array_end;

                ReverseArrayIterator(pointer ptr_begin = nullptr, pointer ptr_curr = nullptr, pointer ptr_end = nullptr):
                    array_beg(ptr_begin), array_curr(ptr_curr), array_end(ptr_end) {}

                ReverseArrayIterator(const ReverseArrayIterator & other):
                    array_beg(other.array_beg), array_curr(other.array_curr), array_end(other.array_end) {}

                ReverseArrayIterator & operator=(const ReverseArrayIterator & other)
                {
                    this->array_beg = other.array_beg;
                    this->array_curr = other.array_curr;
                    this->array_end = other.array_end;
                    return *this;
                }

                ReverseArrayIterator & operator++()
                {
                    --this->array_curr;
                    return *this;
                }

                ReverseArrayIterator & operator--()
                {
                    ++this->array_curr;
                    return *this;
                }

                ReverseArrayIterator operator++(int)
                {
                    ReverseArrayIterator ret(this);
                    ++(*this);
                    return ret;
                }

                ReverseArrayIterator operator--(int)
                {
                    ReverseArrayIterator ret(this);
                    --(*this);
                    return ret;
                }

                difference_type operator-(const ReverseArrayIterator & rhs) const { return this->array_curr - rhs.array_curr; }

                ReverseArrayIterator operator+(ssize_t i)
                {
                    return ReverseArrayIterator(this->array_beg, this->array_curr - i, this->array_end);
                }

                ReverseArrayIterator operator-(ssize_t i)
                {
                    return ReverseArrayIterator(this->array_beg, this->array_curr + i, this->array_end);
                }

                ReverseArrayIterator & operator+=(const std::ptrdiff_t ptr_diff)
                {
                    this->array_curr -= ptr_diff;
                    return *this;
                }

                ReverseArrayIterator & operator-=(const std::ptrdiff_t ptr_diff)
                {
                    this->array_curr += ptr_diff;
                    return *this;
                }

                bool operator==(const ReverseArrayIterator & rhs) const { return this->array_curr == rhs.array_curr; }
                bool operator!=(const ReverseArrayIterator & rhs) const { return !(*this == rhs); }

                bool operator<(const ReverseArrayIterator & rhs) const { return this->array_curr > rhs.array_curr; }
                bool operator<=(const ReverseArrayIterator & rhs) const { return this->array_curr >= rhs.array_curr; }
                bool operator>(const ReverseArrayIterator & rhs) const { return !(*this <= rhs); }
                bool operator>=(const ReverseArrayIterator & rhs) const { return !(*this < rhs); }

                reference operator*()
                {
                    this->_check_pos(this->array_curr);
                    return *this->array_curr;
                }

            private:
                void _check_range(ssize_t movement) const { return this->_check_pos(this->array_curr + movement); }
                void _check_pos(pointer pos) const
                {
                    if (pos < this->array_beg && pos > this->array_end)
                        throw std::out_of_range("Array::reverse_iterator: iterator out of range");
                }
        };

        typedef ReverseArrayIterator<T> reverse_iterator;
        typedef ReverseArrayIterator<const T> const_reverse_iterator;

        reverse_iterator rbegin()
        {
            return reverse_iterator(this->data(),
                                    this->data() + this->size() - 1,
                                    this->data() + this->size());
        }
        reverse_iterator rend()
        {
            return reverse_iterator(this->data(),
                                    this->data() - 1,
                                    this->data() + this->size());
        }

        const_reverse_iterator crbegin()
        {
            return const_reverse_iterator(this->data(),
                                            this->data() + this->size() - 1,
                                            this->data() + this->size());
        }

        const_reverse_iterator crend()
        {
            return const_reverse_iterator(this->data(),
                                            this->data() - 1,
                                            this->data() + this->size());
        }

        // end of Array<T> reverse iterator

    protected:
        // should be called before anything else
        void _init()
        {
            _buf_ptr = nullptr;
            _size = 0;
            _capacity = 0;
            _has_ownership = false;
        }

        T *_buf_ptr;
        size_t _size;
        size_t _capacity;
        // ownership of pointer
        bool _has_ownership;

        static size_t added_resize_capacity;
};
// end of class Array<T>

// added capacity when resizing for less resizes
template <typename T>
size_t Array<T>::added_resize_capacity = 1;

// typedef for types
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

// specialization of char array -> char * can be strlen
class ArrStr: public ArrChar
{
    public:
        using ArrChar::ArrChar;
        using ArrChar::is_equal;
        using ArrChar::copy_from;
        using ArrChar::assign;
        using ArrChar::push_back;
        using ArrChar::from;
        using ArrChar::from_string;
        using ArrChar::to_string;

        std::string to_string([[maybe_unused]] char delimiter = '\0') const
        {
            if (this->size() > 0 && this->cdata()[this->size() - 1] == '\0')
                return std::string(this->cdata(), this->size() - 1);
            return std::string(this->cdata(), this->size());
        }

        // char *

        ArrStr(const char *str)
        {
            this->_init();
            this->push_back(str);
        }

        bool is_equal(const char *str) const
        {
            return this->is_equal(str, strlen(str));
        }

        bool copy_from(const char *str, size_t from = 0)
        {
            return this->copy_from(str, strlen(str), from);
        }

        bool assign(char *str)
        {
            size_t len = strlen(str);
            return this->assign(str, len, len);
        }

        bool assign(char *str, size_t capacity)
        {
            size_t len = strlen(str);
            return this->assign(str, len, capacity);            
        }

        bool push_back(const char *str)
        {
            return this->push_back(str, strlen(str));
        }

        bool from(const char *str)
        {
            return this->from(reinterpret_cast<const uint8_t *>(str), strlen(str));
        }

        // std::string
        
        ArrStr(const std::string & str)
        {
            this->_init();
            this->push_back(str);
        }

        bool is_equal(const std::string & str) const
        {
            return this->is_equal(str.c_str(), str.size());
        }

        bool copy_from(const std::string & str, size_t from = 0)
        {
            return this->copy_from(str.c_str(), str.size(), from);
        }

        bool assign(std::string & str)
        {
            return this->assign(str, str.capacity());
        }

        bool assign(std::string & str, size_t capacity)
        {
            return this->assign(str.data(), str.size(), capacity);            
        }

        bool push_back(const std::string & str)
        {
            return this->push_back(str.c_str(), str.size());
        }

        bool from(const std::string & str)
        {
            return this->from(reinterpret_cast<const uint8_t *>(str.c_str()), str.size());
        }

        bool from_string(const std::string & data, [[maybe_unused]] const char *delimiters = "")
        {
            this->delete_buffer();
            this->push_back(data);
            return true;
        }


};
// end of class ArrStr

// array utils for array manipulation
class ArrayUtil
{
    public:
        static ArrByte *merge_to_byte_array(const std::vector<const IArray *> & arrays)
        {
            return ArrayUtil::merge_to_array<int8_t>(arrays);
        }

        static ArrUByte *merge_to_ubyte_array(const std::vector<const IArray *> & arrays)
        {
            return ArrayUtil::merge_to_array<uint8_t>(arrays);
        }

        // returns a single allocated Array<T> containing contiguous memory of every arrays of vector
        template <typename T>
        static Array<T> *merge_to_array(const std::vector<const IArray *> & arrays)
        {
            size_t total = 0;
            for (const IArray *array: arrays)
                total += array->byte_size();
            Array<T> *ret = new Array<T>();
            if (ret->resize(total * ret->data_size()) == false)
            {
                delete ret;
                return nullptr;
            }
            size_t copy_idx = 0;
            for (const IArray *array: arrays)
            {
                if (ret->copy_from(*array, copy_idx) == false)
                {
                    delete ret;
                    return nullptr;
                }
                copy_idx += array->byte_size();
            }
            return ret;
        }

        /**
         * @brief assign pointers of a distributing array to a list of arrays
         *  distributing_array: size 11 bytes [0x00 ... 0x0b]
         *  assigned_arrays: [ [float_array, 2], [short_array, 1] ]
         *  starting_offset = 1
         * 
         * outputs:
         *  float_array (4 bytes): size 2 [0x01 ... 0x09]
         *  short_array (2 bytes): size 1 [0x09 ... 0x0b]
         * 
         * @param distributing_array the array containing the buffer to distribute to other arrays
         * @param assigned_arrays a vector of array-size pairs that will hold sequentially distributed pointer
         * @param starting_offset the starting byte offset of distributing_array starting byte distribution
         * @return true distribution is done
         * @return false either starting_offset is too high or distributing_array has not enough
         *  capacity to fill assigned_arrays
         */
        static bool distribute_array(IArray & distributing_array,
                                        const std::vector<std::pair<IArray *, size_t>> assigned_arrays, 
                                        size_t starting_offset = 0)
        {
            if (starting_offset > distributing_array.byte_capacity())
            {
                LOG_ERROR("ArrayUtil: starting offset is beyond distributing array capacity (%lu > %lu)",
                            starting_offset, distributing_array.byte_capacity());
                return false;
            }
            size_t total = 0;
            for (const auto & pair: assigned_arrays)
                total += pair.second * pair.first->data_size();
            if ((total + starting_offset) > distributing_array.byte_capacity())
            {
                LOG_ERROR("ArrayUtil: total distribution exceed array capacity (%lu > %lu)",
                            total + starting_offset, distributing_array.byte_capacity());
                return false;
            }
            size_t distributed_byte_size;
            size_t offset_idx = starting_offset;
            for (const auto & pair: assigned_arrays)
            {
                distributed_byte_size = pair.second * pair.first->data_size();
                pair.first->assign_bytes(distributing_array.buf() + offset_idx, distributed_byte_size);
                offset_idx += distributed_byte_size;
            }
            return true;
        }

        static IArray *clone_array(const IArray & to_clone)
        {
            IArray *ret = ArrayUtil::create_from_type(to_clone.data_type());
            ret->resize(to_clone.size());
            ret->copy_from_bytes(to_clone.cbuf(), to_clone.byte_size());
            return ret;
        }

        static IArray *create_from_type(Type dt, size_t size = 0)
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
                case DSTRING:
                    return new ArrStr(size);
                default:
                    break ;
            }
            return nullptr;
        }

        template <typename T>
        static inline Array<T> *cast_array(IArray *ptr)
        {
            return dynamic_cast<Array<T> *>(ptr);
        }

        template <typename T>
        static T read_array(IArray & uncasted_array, size_t idx)
        {
            Array<T> *arr = ArrayUtil::cast_array<T>(&uncasted_array);
            if (arr == nullptr)
            {
                throw std::invalid_argument(
                    Str::format("ArrayUtil::read_array wrong type - invalid cast: %s != %s",
                                uncasted_array.data_type_to_string().c_str(),
                                Datatype::datatype_to_string(Datatype::type_to_datatype<T>()).c_str())
                );
            }
            return arr->at(idx);
        }

        template <typename T>
        static T read_array(IArray *uncasted_array, size_t idx)
        {
            return ArrayUtil::read_array<T>(*uncasted_array, idx);
        }

        template <typename T>
        static bool write_array(IArray & uncasted_array, size_t idx, T value)
        {
            if (idx >= uncasted_array.capacity())
                return false;
            Array<T> *arr = ArrayUtil::cast_array<T>(&uncasted_array);
            if (arr == nullptr)
            {
                throw std::invalid_argument(
                    Str::format("ArrayUtil::write_array wrong type - invalid cast: %s != %s",
                                uncasted_array.data_type_to_string().c_str(),
                                Datatype::datatype_to_string(Datatype::type_to_datatype<T>()).c_str())
                );
            }
            arr->set(idx, value);
            return true;
        }

        template <typename T>
        static bool write_array(IArray *uncasted_array, size_t idx, T value)
        {
            return ArrayUtil::write_array<T>(*uncasted_array, idx, value);
        }

    private:
        ArrayUtil() {};
        ~ArrayUtil() {};
};
// end of class ArrayUtil

}

#endif 