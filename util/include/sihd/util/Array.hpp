#ifndef __SIHD_UTIL_ARRAY_HPP__
# define __SIHD_UTIL_ARRAY_HPP__

# include <sihd/util/Logger.hpp>
# include <sihd/util/IArray.hpp>
# include <sihd/util/Splitter.hpp>

# include <utility>
# include <cstdint>
# include <stdexcept>
# include <memory>
# include <algorithm>

# include <string.h>
# include <strings.h>

namespace sihd::util
{

SIHD_LOGGER;

template <typename T>
class Array: public IArray, public ICloneable<Array<T>>
{
    public:
        using value_type = T;

        Array()
        {
            _buf_ptr = nullptr;
            _size = 0;
            _capacity = 0;
            _has_ownership = false;
        };

        Array(const T *data, size_t size): Array()
        {
            this->reserve(size);
            this->push_back(data, size);
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        Array(const char *str): Array(str, strlen(str))
        {
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        Array(std::string view): Array(view.data(), view.size())
        {
        }

        Array(size_t capacity): Array()
        {
            if (capacity > 0)
                this->reserve(capacity);
        }

        // containers constructors

        Array(std::initializer_list<T> list): Array()
        {
            this->reserve(list.size());
            for (const T & value: list)
                this->push_back(value);
        }

        Array(const std::vector<T> & list): Array()
        {
            this->reserve(list.size());
            for (const T & value: list)
                this->push_back(value);
        }

        template <size_t SIZE>
        Array(const std::array<T, SIZE> & list): Array()
        {
            this->reserve(SIZE);
            for (const T & value: list)
                this->push_back(value);
        }

        // copy constructor

        Array(const Array<T> & array): Array()
        {
            this->from(array);
        }

        Array(Array<T> && other): Array()
        {
            *this = std::move(other);
        }

        Array & operator=(const Array<T> & other)
        {
            if (&other != this)
            {
                _buf_ptr = nullptr;
                _size = 0;
                _capacity = 0;
                _has_ownership = false;
                this->from(other);
            }
            return *this;
        }

        Array & operator=(Array<T> && other)
        {
            if (&other != this)
            {
                _buf_ptr = other._buf_ptr;
                _capacity = other._capacity;
                _size = other._size;
                _has_ownership = other._has_ownership;
                other._buf_ptr = nullptr;
                other._capacity = 0;
                other._size = 0;
            }
            return *this;
        };

        virtual ~Array()
        {
            this->delete_buffer();
        };

        // IArray

        uint8_t *buf() { return (uint8_t *)_buf_ptr; }
        const uint8_t *cbuf() const { return (uint8_t *)_buf_ptr; }

        size_t byte_index(size_t index) const { return index * sizeof(T); }

        uint8_t *buf_at(size_t index) { return (uint8_t *)&_buf_ptr[index]; }
        const uint8_t *cbuf_at(size_t index) const { return (uint8_t *)&_buf_ptr[index]; }

        size_t data_size() const { return sizeof(T); }

        size_t size() const { return _size; }
        size_t byte_size() const { return _size * sizeof(T); }

        size_t capacity() const { return _capacity; }
        size_t byte_capacity() const { return _capacity * sizeof(T); }

        Type data_type() const { return Types::to_type<T>(); }
        const char *data_type_to_string() const { return Types::type_to_string(this->data_type()); }

        bool copy_from_bytes(const void *buf, size_t size, size_t byte_offset = 0)
        {
            if ((size + byte_offset) > this->byte_size())
                return false;
            memcpy(this->buf() + byte_offset, buf, size);
            return true;
        }

        bool copy_from_bytes(const IArray & arr, size_t byte_offset = 0)
        {
            return this->copy_from_bytes(arr.cbuf(), arr.byte_size(), byte_offset);
        }

        bool copy_from_bytes(const IArrayView & arr, size_t byte_offset = 0)
        {
            return this->copy_from_bytes(arr.buf(), arr.byte_size(), byte_offset);
        }

        bool copy_to_bytes(void *buf, size_t size, size_t byte_offset) const
        {
            if (size + byte_offset > this->byte_size())
                return false;
            memcpy(buf, this->cbuf() + byte_offset, size);
            return true;
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool from_string(std::string_view data)
        {
            this->delete_buffer();
            return this->push_back(data);
        }

        bool from_string(std::string_view data, const char *delimiters)
        {
            // can be optimized with string views
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

        bool from(const IArrayView & arr)
        {
            return this->from_bytes(arr.buf(), arr.byte_size());
        }

        bool from(const IArray & arr)
        {
            return this->from_bytes(arr.cbuf(), arr.byte_size());
        }

        bool from_bytes(const void *buf, size_t byte_size)
        {
            if (byte_size % this->data_size() != 0)
            {
                SIHD_LOG_DEBUG("Array::byte_reserve cannot reserve - %lu not divisible by data size %lu",
                                byte_size, this->data_size());
                return false;
            }
            return this->from((const T *)buf, byte_size / this->data_size());
        }

        bool assign_bytes(void *buf, size_t size)
        {
            return this->assign_bytes(buf, size, size);
        }

        bool byte_resize(size_t byte_size)
        {
            if (byte_size % this->data_size() != 0)
            {
                SIHD_LOG_DEBUG("Array::byte_resize cannot resize - %lu not divisible by data size %lu",
                                byte_size, this->data_size());
                return false;
            }
            return this->resize(byte_size / this->data_size());
        }

        bool byte_reserve(size_t byte_size)
        {
            if (byte_size % this->data_size() != 0)
            {
                SIHD_LOG_DEBUG("Array::byte_reserve cannot reserve - %lu not divisible by data size %lu",
                                byte_size, this->data_size());
                return false;
            }
            return this->reserve(byte_size / this->data_size());
        }

        // delete internal buffer if exists then sets it to bytes buffer buf - does not take ownership
        bool assign_bytes(void *buf, size_t byte_size, size_t byte_capacity)
        {
            if (byte_size % this->data_size() != 0)
            {
                SIHD_LOG_DEBUG("Array::assign_bytes cannot assign buffer - size %lu not divisible by %lu",
                                byte_size, this->data_size());
                return false;
            }
            if (byte_capacity % this->data_size() != 0)
            {
                SIHD_LOG_DEBUG("Array::assign_bytes cannot assign buffer - capacity %lu not divisible by %lu",
                                byte_capacity, this->data_size());
                return false;
            }
            return this->assign((T *)buf, byte_size / this->data_size(), byte_capacity / this->data_size());
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
                T *new_buf = new T[capacity]();
                if (new_buf == nullptr)
                    return false;
                size_t new_size = std::min(_size, capacity);
                memcpy((void *)new_buf, (const void *)_buf_ptr, new_size * this->data_size());
                this->delete_buffer();
                _buf_ptr = new_buf;
                _size = new_size;
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

        bool is_same_type(const IArrayView & arr) const
        {
            return this->data_type() == arr.data_type();
        }

        bool is_bytes_equal(const void *buf, size_t size, size_t byte_offset = 0) const
        {
            if (byte_offset > this->byte_size())
                return false;
            if ((this->byte_size() - byte_offset) > size)
                return false;
            return memcmp(this->cbuf() + byte_offset, buf, this->byte_size() - byte_offset) == 0;
        }

        bool is_bytes_equal(const IArray & arr, size_t byte_offset = 0) const
        {
            return this->is_bytes_equal(arr.cbuf(), arr.byte_size(), byte_offset);
        }

        bool is_bytes_equal(const IArrayView & arr, size_t byte_offset = 0) const
        {
            return this->is_bytes_equal(arr.buf(), arr.byte_size(), byte_offset);
        }

        std::string hexdump(char delimiter = ' ') const
        {
            return Str::hexdump(_buf_ptr, this->byte_size(), delimiter);
        }

        std::string to_string() const
        {
            if constexpr (std::is_same_v<T, char>)
            {
                if (this->size() > 0 && this->cdata()[this->size() - 1] == '\0')
                    return std::string(this->cdata(), this->size() - 1);
                return std::string(this->cdata(), this->size());
            }
            else
            {
                std::string s;
                s.reserve(_size);
                size_t i = 0;
                while (i < _size)
                {
                    s += std::to_string(_buf_ptr[i]);
                    ++i;
                }
                return s;
            }
        }

        std::string to_string(char delimiter) const
        {
            std::string s;
            // trying to reserve at least 1 char by element + delimiters (if there are)
            s.reserve(_size + std::max(0, int(_size - 2)));
            size_t i = 0;
            while (i < _size)
            {
                if (i > 0)
                    s += delimiter;
                if constexpr (std::is_same_v<T, char>)
                    s += _buf_ptr[i];
                else
                    s += std::to_string(_buf_ptr[i]);
                ++i;
            }
            return s;
        }

        void clear()
        {
            this->resize(0);
        }

        std::string cpp_str() const
        {
            return _buf_ptr != nullptr ? std::string((char *)_buf_ptr, this->byte_size()) : "";
        }

        std::string_view cpp_str_view() const
        {
            return _buf_ptr != nullptr ? std::string_view((char *)_buf_ptr, this->byte_size()) : "";
        }

        // ICloneable

        Array<T> *clone() const
        {
            Array<T> *cloned = new Array<T>();
            if (cloned != nullptr)
                cloned->from(*this);
            return cloned;
        }

        IArray *clone_array() const
        {
            return this->clone();
        }

        // Class methods

        T *data() { return _buf_ptr; }
        const T *cdata() const { return _buf_ptr; }

        void set_buffer_ownership(bool active)
        {
            _has_ownership = active;
        }

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

        std::vector<T> cpp_vector() const
        {
            return _buf_ptr != nullptr
                ? std::vector<T>(_buf_ptr, _buf_ptr + _size)
                : std::vector<T>();
        }

        template <size_t ARRAY_SIZE>
        std::array<T, ARRAY_SIZE> cpp_array() const
        {
            std::array<T, ARRAY_SIZE> arr;
            if (_buf_ptr != nullptr && _size >= ARRAY_SIZE)
                memcpy(arr.data(), _buf_ptr, ARRAY_SIZE * this->data_size());
            return arr;
        }

        bool is_equal(const std::vector<T> & vec, size_t byte_offset = 0) const
        {
            return this->is_equal(vec.data(), vec.size(), byte_offset);
        }

        template <size_t ARRAY_SIZE>
        bool is_equal(const std::array<T, ARRAY_SIZE> & array, size_t byte_offset = 0) const
        {
            return this->is_equal(array.data(), array.size(), byte_offset);
        }

        bool is_equal(const Array<T> & arr, size_t byte_offset = 0) const
        {
            return this->is_equal(arr.cdata(), arr.size(), byte_offset);
        }

        bool is_equal(std::initializer_list<T> init, size_t byte_offset = 0) const
        {
            auto it = init.begin();
            return this->is_equal(&(*it), init.size(), byte_offset);
        }

        // compares memory from internal buffer and array of size
        bool is_equal(const T *arr, size_t size, size_t byte_offset = 0) const
        {
            return this->is_bytes_equal(arr, size * this->data_size(), byte_offset * this->data_size());
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool is_equal(std::string_view str, size_t byte_offset = 0) const
        {
            return this->is_bytes_equal(str.data(), str.size(), byte_offset);
        }

        bool from(const std::vector<T> & vec)
        {
            return this->from(vec.data(), vec.size());
        }

        template <size_t ARRAY_SIZE>
        bool from(const std::array<T, ARRAY_SIZE> & array)
        {
            return this->from(array.data(), array.size());
        }

        bool from(const T *arr, size_t size)
        {
            if (this->new_buffer(size))
            {
                memcpy(_buf_ptr, arr, size * this->data_size());
                _size = size;
            }
            return _buf_ptr != nullptr;
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool from(std::string_view str)
        {
            return this->from_bytes(str.data(), str.size());
        }

        bool copy_from(const std::vector<T> & vec, size_t byte_offset = 0)
        {
            return this->copy_from(vec.data(), vec.size(), byte_offset);
        }

        template <size_t ARRAY_SIZE>
        bool copy_from(const std::array<T, ARRAY_SIZE> & array, size_t byte_offset = 0)
        {
            return this->copy_from(array.data(), array.size(), byte_offset);
        }

        // copies values from array
        bool copy_from(const T *arr, size_t size, size_t byte_offset = 0)
        {
            return this->copy_from_bytes(arr, size * this->data_size(), byte_offset * this->data_size());
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool copy_from(const char *str, size_t byte_offset = 0)
        {
            return this->copy_from_bytes(str, strlen(str), byte_offset);
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool copy_from(std::string_view view, size_t byte_offset = 0)
        {
            return this->copy_from_bytes(view.data(), view.size(), byte_offset);
        }

        // delete internal buffer if exists then sets it to vector buf - does not take ownership
        bool assign(const std::vector<T> & vec)
        {
            return this->assign(vec.data(), vec.size());
        }

        // delete internal buffer if exists then sets it to array buf - does not take ownership
        template <size_t ARRAY_SIZE>
        bool assign(const std::array<T, ARRAY_SIZE> & array)
        {
            return this->assign(array.data(), array.size());
        }

        // delete internal buffer if exists then sets it to array buf - does not take ownership
        bool assign(T *arr, size_t size)
        {
            return this->assign(arr, size, size);
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool assign(char *str)
        {
            size_t size = strlen(str);
            return this->assign(str, size, size);
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool assign(std::string & str)
        {
            return this->assign(str.data(), str.size(), str.size());
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

        bool push_back(const std::vector<T> & vec)
        {
            return this->push_back(vec.data(), vec.size());
        }

        template <size_t ARRAY_SIZE>
        bool push_back(const std::array<T, ARRAY_SIZE> & array)
        {
            return this->push_back(array.data(), array.size());
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool push_back(std::string_view str)
        {
            return this->push_back(str.data(), str.size());
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool push_back(const char *str)
        {
            return this->push_back(str, strlen(str));
        }

        // push whole array buf of size at the end of the internal buffer
        bool push_back(const T *buf, size_t size)
        {
            return this->insert(buf, size, _size);
        }

        // push a single value at the end of the array
        bool push_back(const T & value)
        {
            return this->insert(value, _size);
        }

        bool push_front(const std::vector<T> & vec)
        {
            return this->push_front(vec.data(), vec.size());
        }

        template <size_t ARRAY_SIZE>
        bool push_front(const std::array<T, ARRAY_SIZE> & array)
        {
            return this->push_front(array.data(), array.size());
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool push_front(std::string_view str)
        {
            return this->push_front(str.data(), str.size());
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool push_front(const char *str)
        {
            return this->push_front(str, strlen(str));
        }

        // push whole array buf of size at the front of the internal buffer
        bool push_front(const T *buf, size_t size)
        {
            return this->insert(buf, size, 0);
        }

        // push a single value at the beginning of the array
        bool push_front(const T & value)
        {
            return this->insert(value, 0);
        }

        bool insert(const T *buf, size_t size, size_t idx)
        {
            // ex: insert({4, 5, 6}, 3, 2)
            // internal buffer: {0, 1, 2, 3}

            if (idx > _size)
                return false;
            if (_size + size > _capacity)
                this->reserve(_size + std::max(Array::added_resize_capacity, size));
            if (_buf_ptr != nullptr)
            {
                // have to move 2 and 3 -> 3 * sizeof(int) to the right to leave room for insertion
                size_t len_move = (_size - idx) * this->data_size();
                // no need to move if idx == _size
                if (len_move > 0)
                {
                    memmove((uint8_t *)(_buf_ptr + (idx + size)),
                            (uint8_t *)(_buf_ptr + idx),
                            len_move);
                    // -> {0, 1, _, _, _, 2, 3}
                }
                memcpy((uint8_t *)(_buf_ptr + idx), buf, size * this->data_size());
                // -> {0, 1, 4, 5}
                _size += size;
                // -> {0, 1, 4, 5, 6, 2, 3}
            }
            return _buf_ptr != nullptr;
        }

        bool insert(const T & value, size_t idx)
        {
            return this->insert(&value, 1, idx);
        }

        // remove value at idx and returns it
        T pop(size_t idx)
        {
            // ex: pop(1)
            // internal buffer: {5, 10, 15}

            // get value - might throw - checks idx
            T ret = this->at(idx);
            size_t len = (_size - (idx + 1)) * this->data_size();

            // moving all remaining buffer to current idx
            memmove((uint8_t *)(_buf_ptr + idx),
                    (uint8_t *)(_buf_ptr + idx + 1),
                    len);
            // -> {5, 15, 15}

            // set to 0 remaining of memory
            memset((uint8_t *)(_buf_ptr + _size - 1), 0, this->data_size());
            // -> {5, 15, 0}
            --_size;
            // -> {5, 15}
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
                bool operator>(const ArrayIterator & rhs) const { return !(*this <= rhs); }
                bool operator<=(const ArrayIterator & rhs) const { return this->array_curr <= rhs.array_curr; }
                bool operator>=(const ArrayIterator & rhs) const { return !(*this < rhs); }

                reference operator*()
                {
                    this->_check_pos(this->array_curr);
                    return *this->array_curr;
                }

                size_t idx()
                {
                    this->_check_pos(this->array_curr);
                    return this->array_curr - this->array_beg;
                }

            private:
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

        const_iterator cbegin() const { return const_iterator(this->cdata(), this->cdata(), this->cdata() + this->size()); }
        const_iterator cend() const { return const_iterator(this->cdata(), this->cdata() + this->size(), this->cdata() + this->size()); }

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

                difference_type operator-(const ReverseArrayIterator & rhs) const { return rhs.array_curr - this->array_curr; }

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
                bool operator>(const ReverseArrayIterator & rhs) const { return !(*this <= rhs); }
                bool operator<=(const ReverseArrayIterator & rhs) const { return this->array_curr >= rhs.array_curr; }
                bool operator>=(const ReverseArrayIterator & rhs) const { return !(*this < rhs); }

                reference operator*()
                {
                    this->_check_pos(this->array_curr);
                    return *this->array_curr;
                }

                size_t idx()
                {
                    this->_check_pos(this->array_curr);
                    return this->array_curr - this->array_beg;
                }

            private:
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

        const_reverse_iterator crbegin() const
        {
            return const_reverse_iterator(this->cdata(),
                                            this->cdata() + this->size() - 1,
                                            this->cdata() + this->size());
        }

        const_reverse_iterator crend() const
        {
            return const_reverse_iterator(this->cdata(),
                                            this->cdata() - 1,
                                            this->cdata() + this->size());
        }

        static constexpr size_t npos = size_t(-1);

        // end of Array<T> reverse iterator

        size_t find(const T value) const
        {
            const_iterator it = std::find(this->cbegin(), this->cend(), value);
            if (it == this->cend())
                return Array<T>::npos;
            return it.idx();
        }

        size_t rfind(const T value) const
        {
            const_reverse_iterator it = std::find(this->crbegin(), this->crend(), value);
            if (it == this->crend())
                return Array<T>::npos;
            return it.idx();
        }

    protected:
        T *_buf_ptr;
        size_t _size;
        size_t _capacity;
        // ownership of pointer
        bool _has_ownership;

        static size_t added_resize_capacity;
};
// end of class Array<T>

// added capacity when resizing for less allocations
template <typename T>
size_t Array<T>::added_resize_capacity = 4;

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
                if (ret->copy_from_bytes(*array, copy_idx) == false)
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
            if (starting_offset > distributing_array.byte_size())
            {
                SIHD_LOG_DEBUG("ArrayUtil: starting offset is beyond distributing array size (%lu > %lu)",
                            starting_offset, distributing_array.byte_size());
                return false;
            }
            size_t total = 0;
            for (const auto & pair: assigned_arrays)
                total += pair.second * pair.first->data_size();
            if ((total + starting_offset) > distributing_array.byte_size())
            {
                SIHD_LOG_DEBUG("ArrayUtil: total distribution exceed array size (%lu > %lu)",
                            total + starting_offset, distributing_array.byte_size());
                return false;
            }
            size_t distributed_byte_size;
            size_t offset_idx = starting_offset;
            for (const auto & pair: assigned_arrays)
            {
                distributed_byte_size = pair.second * pair.first->data_size();
                if (pair.first->assign_bytes(distributing_array.buf() + offset_idx, distributed_byte_size) == false)
                    return false;
                offset_idx += distributed_byte_size;
            }
            return true;
        }

        static IArray *create_from_type(Type dt, size_t size = 0)
        {
            switch (dt)
            {
                case TYPE_BOOL:
                    return new ArrBool(size);
                case TYPE_CHAR:
                    return new ArrChar(size);
                case TYPE_BYTE:
                    return new ArrByte(size);
                case TYPE_UBYTE:
                    return new ArrUByte(size);
                case TYPE_SHORT:
                    return new ArrShort(size);
                case TYPE_USHORT:
                    return new ArrUShort(size);
                case TYPE_INT:
                    return new ArrInt(size);
                case TYPE_UINT:
                    return new ArrUInt(size);
                case TYPE_LONG:
                    return new ArrLong(size);
                case TYPE_ULONG:
                    return new ArrULong(size);
                case TYPE_FLOAT:
                    return new ArrFloat(size);
                case TYPE_DOUBLE:
                    return new ArrDouble(size);
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
        static inline const Array<T> *cast_array(const IArray *ptr)
        {
            return dynamic_cast<const Array<T> *>(ptr);
        }

        template <typename T>
        static bool read_array_into(const IArray & arr, size_t idx, T & val)
        {
            return arr.copy_to_bytes(&val, sizeof(T), arr.byte_index(idx));
        }

        template <typename T>
        static bool read_array_into(const IArray *arr, size_t idx, T & val)
        {
            return ArrayUtil::read_array_into<T>(*arr, idx, val);
        }

        template <typename T>
        static T read_array(const IArray & arr, size_t idx)
        {
            T ret;
            if ( ArrayUtil::read_array_into<T>(arr, idx, ret) == false)
            {
                throw std::invalid_argument(
                    Str::format("ArrayUtil::read_array cannot copy data from idx %lu into type '%s' (array type: '%s')",
                                idx,
                                Types::to_string<T>(),
                                arr.data_type_to_string())
                );
            }
            return ret;
        }

        template <typename T>
        static T read_array(const IArray *arr, size_t idx)
        {
            return ArrayUtil::read_array<T>(*arr, idx);
        }

        template <typename T>
        static bool write_array(IArray & arr, size_t idx, T value)
        {
            return arr.copy_from_bytes(&value, sizeof(T), arr.byte_index(idx));
        }

        template <typename T>
        static bool write_array(IArray *arr, size_t idx, T value)
        {
            return ArrayUtil::write_array<T>(*arr, idx, value);
        }

    private:
        ~ArrayUtil() {};
};
// end of class ArrayUtil

}

#endif