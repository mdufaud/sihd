#ifndef __SIHD_UTIL_ARRAY_HPP__
#define __SIHD_UTIL_ARRAY_HPP__

#include <cstdint>   // int8_t
#include <cstring>   // mem* str*
#include <stdexcept> // out of range
#include <utility>   // std::enable_if

#include <sihd/util/IArray.hpp>
#include <sihd/util/ICloneable.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/traits.hpp>

namespace sihd::util
{

// this array implementation relies on memcpy/memcmp
template <traits::TriviallyCopyable T>
class Array: public IArray,
             public ICloneable<Array<T>>
{
    public:
        using value_type = T;
        using size_type = size_t;
        using pointer = T *;
        using const_pointer = const T *;
        using reference = T &;
        using const_reference = const T &;

        static constexpr size_t npos = size_t(-1);

        Array()
        {
            _buf_ptr = nullptr;
            _size = 0;
            _capacity = 0;
            _has_ownership = false;
        };

        Array(const T *data, size_t size): Array()
        {
            this->_internal_reserve(size, false);
            this->push_back(data, size);
        }

        Array(size_t capacity): Array()
        {
            if (capacity > 0)
                this->_internal_reserve(capacity);
        }

        /*********************************************************************/
        /* contiguous std containers constructors */
        /*********************************************************************/

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        Array(std::string_view view): Array(view.data(), view.size())
        {
        }

        Array(std::initializer_list<T> list): Array()
        {
            auto it = list.begin();
            this->push_back(&(*it), list.size());
        }

        template <traits::Iterable Container>
        Array(const Container & container): Array()
        {
            this->reserve(std::distance(std::begin(container), std::end(container)));
            for (const auto & item : container)
                this->push_back(item);
        }

        /*********************************************************************/
        /* copy constructor */
        /*********************************************************************/

        // you have to check for good memory allocation here
        Array(const Array<T> & array): Array() { this->from(array); }

        Array(Array<T> && other): Array() { *this = std::move(other); }

        /*********************************************************************/
        /* operator= */
        /*********************************************************************/

        // you have to check for good memory allocation after here
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

        ~Array() { this->delete_buffer(); };

        /*********************************************************************/
        /* information */
        /*********************************************************************/

        operator bool() const { return _buf_ptr != nullptr; }

        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        operator std::string() const
        {
            return std::string(this->data(), this->byte_size());
        }

        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        operator std::string_view() const
        {
            return std::string_view(this->data(), this->byte_size());
        }

        uint8_t *buf() { return (uint8_t *)_buf_ptr; }
        const uint8_t *buf() const { return (uint8_t *)_buf_ptr; }

        size_t byte_index(size_t index) const { return index * sizeof(T); }

        uint8_t *buf_at(size_t index) { return (uint8_t *)&_buf_ptr[index]; }
        const uint8_t *buf_at(size_t index) const { return (uint8_t *)&_buf_ptr[index]; }

        size_t data_size() const { return sizeof(T); }

        size_t size() const { return _size; }
        size_t byte_size() const { return _size * sizeof(T); }
        bool empty() const { return _size == 0; }

        size_t capacity() const { return _capacity; }
        size_t byte_capacity() const { return _capacity * sizeof(T); }

        Type data_type() const { return Types::type<T>(); }
        const char *data_type_str() const { return Types::type_str(this->data_type()); }

        /*********************************************************************/
        /* copy_from */
        /*********************************************************************/

        bool copy_from_bytes(const void *buf, size_t size, size_t byte_offset = 0)
        {
            if ((size + byte_offset) > this->byte_size())
                return false;
            memcpy(this->buf() + byte_offset, buf, size);
            return true;
        }

        bool copy_from_bytes(const IArray & arr, size_t byte_offset = 0)
        {
            return this->copy_from_bytes(arr.buf(), arr.byte_size(), byte_offset);
        }

        bool copy_from_bytes(const IArrayView & arr, size_t byte_offset = 0)
        {
            return this->copy_from_bytes(arr.buf(), arr.byte_size(), byte_offset);
        }

        /*********************************************************************/
        /* copy_to */
        /*********************************************************************/

        bool copy_to_bytes(void *buf, size_t size, size_t byte_offset = 0) const
        {
            if (size + byte_offset > this->byte_size())
                return false;
            memcpy(buf, this->buf() + byte_offset, size);
            return true;
        }

        /*********************************************************************/
        /* from */
        /*********************************************************************/

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool from_str(std::string_view data)
        {
            this->delete_buffer();
            return this->push_back(data);
        }

        bool from_str(std::string_view data, std::string_view delimiters)
        {
            if constexpr (std::is_fundamental_v<T>)
            {
                // can be optimized with string views
                Splitter splitter(delimiters);
                const std::vector<std::string> splits = splitter.split(data);
                this->reserve(splits.size());
                this->resize(0);
                for (const std::string & split : splits)
                {
                    T value;
                    if (str::convert_from_string<T>(split, value) == false)
                        return false;
                    this->push_back(value);
                }
            }
            else
            {
                Splitter splitter(delimiters);
                const std::vector<std::string> splits = splitter.split(data);
                this->resize(0);
                if (splits.size() % sizeof(T) != 0)
                    return false;
                uint32_t idx = 0;
                for (const std::string & split : splits)
                {
                    constexpr uint16_t base = 16;
                    uint8_t byte;
                    if (str::convert_from_string<uint8_t>(split, byte, base) == false)
                        return false;
                    if (this->resize((idx / sizeof(T)) + 1) == false)
                        return false;
                    this->buf()[idx] = byte;
                    ++idx;
                }
            }
            return true;
        }

        // throws std::invalid_argument if byte size is not aligned with type size
        bool from_bytes(const IArrayView & arr) { return this->from_bytes(arr.buf(), arr.byte_size()); }

        // throws std::invalid_argument if byte size is not aligned with type size
        bool from_bytes(const IArray & arr) { return this->from_bytes(arr.buf(), arr.byte_size()); }

        // throws std::invalid_argument if byte size is not aligned with type size
        bool from_bytes(const void *buf, size_t byte_size)
        {
            if (byte_size % sizeof(T) != 0)
                throw std::invalid_argument(
                    str::format("Array::from_bytes buffer - %lu not divisible by data size %lu", byte_size, sizeof(T)));
            return this->from((const T *)buf, byte_size / sizeof(T));
        }

        /*********************************************************************/
        /* resize */
        /*********************************************************************/

        // throws std::invalid_argument if byte size is not aligned with type size
        bool byte_resize(size_t byte_size)
        {
            if (byte_size % this->data_size() != 0)
                throw std::invalid_argument(
                    str::format("Array::byte_resize cannot resize - %lu not divisible by data size %lu",
                                byte_size,
                                sizeof(T)));
            return this->resize(byte_size / this->data_size());
        }

        bool resize(size_t size) { return this->_internal_resize(size, true); }

        void clear() { this->resize(0); }

        /*********************************************************************/
        /* reserve */
        /*********************************************************************/

        // throws std::invalid_argument if byte size is not aligned with type size
        bool byte_reserve(size_t byte_size)
        {
            if (byte_size % this->data_size() != 0)
                throw std::invalid_argument(
                    str::format("Array::byte_reserve cannot reserve - %lu not divisible by data size %lu",
                                byte_size,
                                sizeof(T)));
            return this->reserve(byte_size / this->data_size());
        }

        bool reserve(size_t capacity) { return this->_internal_reserve(capacity, true); }

        /*********************************************************************/
        /* assign */
        /*********************************************************************/

        // throws std::invalid_argument if byte size is not aligned with type size
        bool assign_bytes(void *buf, size_t size) { return this->assign_bytes(buf, size, size); }

        // delete internal buffer if exists then sets it to bytes buffer buf - does not take ownership
        // throws std::invalid_argument if byte size or capacity is not aligned with type size
        bool assign_bytes(void *buf, size_t byte_size, size_t byte_capacity)
        {
            if (byte_size % this->data_size() != 0)
                throw std::invalid_argument(
                    str::format("Array::assign_bytes - size %lu not divisible by data size %lu", byte_size, sizeof(T)));
            if (byte_capacity % this->data_size() != 0)
                throw std::invalid_argument(
                    str::format("Array::assign_bytes - capacity %lu not divisible by data size %lu",
                                byte_capacity,
                                sizeof(T)));
            return this->assign((T *)buf, byte_size / this->data_size(), byte_capacity / this->data_size());
        }

        /*********************************************************************/
        /* comparison */
        /*********************************************************************/

        bool is_bytes_equal(const void *buf, size_t size, size_t byte_offset = 0) const
        {
            if (byte_offset > this->byte_size())
                return false;
            if ((this->byte_size() - byte_offset) < size)
                return false;
            return memcmp(this->buf() + byte_offset, buf, size) == 0;
        }

        bool is_bytes_equal(const IArray & arr, size_t byte_offset = 0) const
        {
            return this->is_bytes_equal(arr.buf(), arr.byte_size(), byte_offset);
        }

        bool is_bytes_equal(const IArrayView & arr, size_t byte_offset = 0) const
        {
            return this->is_bytes_equal(arr.buf(), arr.byte_size(), byte_offset);
        }

        bool is_same_type(const IArray & arr) const { return this->data_type() == arr.data_type(); }

        bool is_same_type(const IArrayView & arr) const { return this->data_type() == arr.data_type(); }

        /*********************************************************************/
        /* views */
        /*********************************************************************/

        std::string hexdump(char delimiter = ' ') const { return str::hexdump(_buf_ptr, this->byte_size(), delimiter); }

        std::string str() const
        {
            if constexpr (std::is_same_v<T, char>)
            {
                if (this->size() > 0 && this->data()[this->size() - 1] == '\0')
                    return std::string(this->data(), this->size() - 1);
                return std::string(this->data(), this->size());
            }
            if constexpr (std::is_fundamental_v<T>)
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
            else
                return str::hexdump(_buf_ptr, this->byte_size(), 0);
        }

        std::string str(char delimiter) const
        {
            if constexpr (std::is_fundamental_v<T>)
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
            else
                return str::hexdump(_buf_ptr, this->byte_size(), delimiter);
        }

        /*********************************************************************/
        /* to std containers */
        /*********************************************************************/

        std::string cpp_str() const
        {
            return _buf_ptr != nullptr ? std::string((const char *)_buf_ptr, this->byte_size()) : "";
        }

        std::string_view cpp_str_view() const
        {
            return _buf_ptr != nullptr ? std::string_view((const char *)_buf_ptr, this->byte_size()) : "";
        }

        /*********************************************************************/
        /* clone */
        /*********************************************************************/

        Array<T> *clone() const
        {
            Array<T> *cloned = new Array<T>();
            if (cloned != nullptr)
                cloned->from(*this);
            return cloned;
        }

        IArray *clone_array() const { return this->clone(); }

        /*********************************************************************/
        /* class methods */
        /*********************************************************************/

        T *data() { return _buf_ptr; }
        const T *data() const { return _buf_ptr; }

        void set_buffer_ownership(bool active) { _has_ownership = active; }

        /*********************************************************************/
        /* buffer methods */
        /*********************************************************************/

        // delete internal buffer if it has ownership - set internal buffer to nullptr
        void delete_buffer()
        {
            if (_buf_ptr != nullptr && _has_ownership)
                free(_buf_ptr);
            _size = 0;
            _capacity = 0;
            _buf_ptr = nullptr;
            _has_ownership = false;
        }

        /*********************************************************************/
        /* is_equal */
        /*********************************************************************/

        // container specialization
        template <traits::HasDataSize Container>
        bool is_equal(const Container & container, size_t byte_offset = 0) const
        {
            return this->is_equal(container.data(), container.size(), byte_offset);
        }

        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool is_equal(std::string_view view, size_t byte_offset = 0) const
        {
            return this->is_equal(view.data(), view.size(), byte_offset);
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

        /*********************************************************************/
        /* from */
        /*********************************************************************/

        template <traits::HasDataSize Container>
        bool from(const Container & container)
        {
            return this->from(container.data(), container.size());
        }

        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool from(std::string_view view)
        {
            return this->from(view.data(), view.size());
        }

        bool from(std::initializer_list<T> init)
        {
            auto it = init.begin();
            return this->from(&(*it), init.size());
        }

        bool from(const T *arr, size_t size)
        {
            this->delete_buffer();
            if (this->_internal_reserve(size, false))
            {
                memcpy(_buf_ptr, arr, size * sizeof(T));
                _size = size;
            }
            return _buf_ptr != nullptr;
        }

        /*********************************************************************/
        /* copy_from */
        /*********************************************************************/

        // container specialization
        template <traits::HasDataSize Container>
        bool copy_from(const Container & container, size_t byte_offset = 0)
        {
            return this->copy_from(container.data(), container.size(), byte_offset);
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool copy_from(std::string_view view, size_t byte_offset = 0)
        {
            return this->copy_from(view.data(), view.size(), byte_offset);
        }

        bool copy_from(std::initializer_list<T> init, size_t byte_offset = 0)
        {
            auto it = init.begin();
            return this->copy_from(&(*it), init.size(), byte_offset);
        }

        // copies values from array
        bool copy_from(const T *arr, size_t size, size_t byte_offset = 0)
        {
            return this->copy_from_bytes(arr, size * this->data_size(), byte_offset * this->data_size());
        }

        /*********************************************************************/
        /* assign */
        /*********************************************************************/

        // container specialization
        // delete internal buffer if exists then sets it to container buf - does not take ownership
        template <traits::HasDataSize Container>
        bool assign(Container & container)
        {
            return this->assign(container.data(), container.size());
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool assign(char *str)
        {
            size_t size = str != nullptr ? strlen(str) : 0;
            return this->assign(str, size, size);
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool assign(std::string & str)
        {
            return this->assign(str.data(), str.size(), str.size());
        }

        // delete internal buffer if exists then sets it to array buf - does not take ownership
        bool assign(T *arr, size_t size) { return this->assign(arr, size, size); }

        // delete internal buffer if exists then sets it to array buf - does not take ownership
        bool assign(T *arr, size_t size, size_t capacity)
        {
            if (arr != _buf_ptr)
            {
                this->delete_buffer();
                _buf_ptr = arr;
                _size = size;
                _capacity = capacity;
                _has_ownership = false;
            }
            return true;
        }

        /*********************************************************************/
        /* push_back */
        /*********************************************************************/

        template <traits::HasDataSize Container>
        bool push_back(const Container & container)
        {
            return this->push_back(container.data(), container.size());
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool push_back(std::string_view str)
        {
            return this->push_back(str.data(), str.size());
        }

        bool push_back(std::initializer_list<T> init)
        {
            auto it = init.begin();
            return this->push_back(&(*it), init.size());
        }

        // push whole array buf of size at the end of the internal buffer
        bool push_back(const T *buf, size_t size) { return this->insert(buf, size, _size); }

        // push a single value at the end of the array
        bool push_back(const T & value)
        {
            if (this->_internal_resize(_size + 1, false))
                _buf_ptr[_size - 1] = value;
            return _buf_ptr != nullptr;
        }

        /*********************************************************************/
        /* push_front */
        /*********************************************************************/

        template <traits::HasDataSize Container>
        bool push_front(const Container & container)
        {
            return this->push_front(container.data(), container.size());
        }

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        bool push_front(std::string_view str)
        {
            return this->push_front(str.data(), str.size());
        }

        bool push_front(std::initializer_list<T> init)
        {
            auto it = init.begin();
            return this->push_front(&(*it), init.size());
        }

        // push whole array buf of size at the front of the internal buffer
        bool push_front(const T *buf, size_t size) { return this->insert(buf, size, 0); }

        // push a single value at the beginning of the array
        bool push_front(const T & value) { return this->insert(value, 0); }

        /*********************************************************************/
        /* modify */
        /*********************************************************************/

        bool insert(const T *buf, size_t size, size_t idx)
        {
            // ex: insert({4, 5, 6}, 3, 2)
            // internal buffer: {0, 1, 2, 3}

            if (idx > _size)
                return false;
            if (_size + size > _capacity)
                this->_internal_reserve(_size + size, false);
            if (_buf_ptr != nullptr)
            {
                // have to move 2 and 3 -> 3 * sizeof(int) to the right to leave room for insertion
                size_t len_move = (_size - idx) * this->data_size();
                // no need to move if idx == _size
                if (len_move > 0)
                {
                    memmove((uint8_t *)(_buf_ptr + (idx + size)), (uint8_t *)(_buf_ptr + idx), len_move);
                    // -> {0, 1, _, _, _, 2, 3}
                }
                memcpy((uint8_t *)(_buf_ptr + idx), buf, size * this->data_size());
                // -> {0, 1, 4, 5}
                _size += size;
                // -> {0, 1, 4, 5, 6, 2, 3}
            }
            return _buf_ptr != nullptr;
        }

        bool insert(const T & value, size_t idx) { return this->insert(&value, 1, idx); }

        // remove value at idx and returns it
        T pop(size_t idx)
        {
            // ex: pop(1)
            // internal buffer: {5, 10, 15}

            // get value - might throw - checks idx
            T ret = this->at(idx);
            size_t len = (_size - (idx + 1)) * this->data_size();

            // moving all remaining buffer to current idx
            memmove((uint8_t *)(_buf_ptr + idx), (uint8_t *)(_buf_ptr + idx + 1), len);
            // -> {5, 15, 15}

            // set to 0 remaining of memory
            memset((uint8_t *)(_buf_ptr + _size - 1), 0, this->data_size());
            // -> {5, 15, 0}
            --_size;
            // -> {5, 15}
            return ret;
        }

        T pop_back()
        {
            T ret = this->at(_size - 1);
            --_size;
            return ret;
        }

        T pop_front() { return pop(0); }

        // set value at idx - throws out_of_range
        void set(size_t idx, const T & value)
        {
            if (idx >= _size)
                throw std::out_of_range("Array::set: index exceeds size");
            _buf_ptr[idx] = value;
        }

        T & get(size_t idx)
        {
            if (idx >= _size)
                throw std::out_of_range("Array::get: index exceeds size");
            return _buf_ptr[idx];
        }

        /*********************************************************************/
        /* access */
        /*********************************************************************/

        // data first value
        T front() const { return _buf_ptr[0]; }

        // access last value
        T back() const { return _buf_ptr[_size - 1]; }

        // access value at idx - throws out_of_range
        T at(size_t idx) const
        {
            if (idx >= _size)
                throw std::out_of_range("Array::at: index exceeds size");
            return _buf_ptr[idx];
        }

        // access value arr[idx]
        T operator[](size_t idx) const { return _buf_ptr[idx]; }

        // set value arr[idx] = value
        T & operator[](size_t idx) { return _buf_ptr[idx]; }

        /*********************************************************************/
        /* iterator */
        /*********************************************************************/

        template <typename IteratorType>
        class ArrayIterator
        {
            public:
                // Iterator traits - typedefs and types required to be STL compliant
                using iterator_category = std::random_access_iterator_tag;
                using difference_type = std::ptrdiff_t;
                using value_type = IteratorType;
                using pointer = IteratorType *;
                using reference = IteratorType &;

                pointer array_beg;
                pointer array_curr;
                pointer array_end;

                ArrayIterator(pointer ptr_begin = nullptr, pointer ptr_curr = nullptr, pointer ptr_end = nullptr):
                    array_beg(ptr_begin),
                    array_curr(ptr_curr),
                    array_end(ptr_end)
                {
                }

                ArrayIterator(const ArrayIterator & other):
                    array_beg(other.array_beg),
                    array_curr(other.array_curr),
                    array_end(other.array_end)
                {
                }

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
                    if (this->array_curr<this->array_beg && this->array_curr> this->array_end)
                        throw std::out_of_range("Array::iterator: iterator out of range");
                    return *this->array_curr;
                }

                size_t idx()
                {
                    if (this->array_curr<this->array_beg && this->array_curr> this->array_end)
                        return Array<T>::npos;
                    return this->array_curr - this->array_beg;
                }
        };

        typedef ArrayIterator<T> iterator;
        typedef ArrayIterator<const T> const_iterator;

        iterator begin() { return iterator(this->data(), this->data(), this->data() + this->size()); }
        iterator end() { return iterator(this->data(), this->data() + this->size(), this->data() + this->size()); }

        const_iterator cbegin() const
        {
            return const_iterator(this->data(), this->data(), this->data() + this->size());
        }
        const_iterator cend() const
        {
            return const_iterator(this->data(), this->data() + this->size(), this->data() + this->size());
        }

        /*********************************************************************/
        /* reverse iterator */
        /*********************************************************************/
        template <typename IteratorType>
        class ReverseArrayIterator
        {
            public:
                // Iterator traits - typedefs and types required to be STL compliant
                using iterator_category = std::random_access_iterator_tag;
                using difference_type = std::ptrdiff_t;
                using value_type = IteratorType;
                using pointer = IteratorType *;
                using reference = IteratorType &;

                pointer array_beg;
                pointer array_curr;
                pointer array_end;

                ReverseArrayIterator(pointer ptr_begin = nullptr,
                                     pointer ptr_curr = nullptr,
                                     pointer ptr_end = nullptr):
                    array_beg(ptr_begin),
                    array_curr(ptr_curr),
                    array_end(ptr_end)
                {
                }

                ReverseArrayIterator(const ReverseArrayIterator & other):
                    array_beg(other.array_beg),
                    array_curr(other.array_curr),
                    array_end(other.array_end)
                {
                }

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

                difference_type operator-(const ReverseArrayIterator & rhs) const
                {
                    return rhs.array_curr - this->array_curr;
                }

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
                    if (this->array_curr<this->array_beg && this->array_curr> this->array_end)
                        throw std::out_of_range("Array::reverse_iterator: iterator out of range");
                    return *this->array_curr;
                }

                size_t idx()
                {
                    if (this->array_curr<this->array_beg && this->array_curr> this->array_end)
                        return Array<T>::npos;
                    return this->array_curr - this->array_beg;
                }
        };

        typedef ReverseArrayIterator<T> reverse_iterator;
        typedef ReverseArrayIterator<const T> const_reverse_iterator;

        reverse_iterator rbegin()
        {
            return reverse_iterator(this->data(), this->data() + this->size() - 1, this->data() + this->size());
        }
        reverse_iterator rend()
        {
            return reverse_iterator(this->data(), this->data() - 1, this->data() + this->size());
        }

        const_reverse_iterator crbegin() const
        {
            return const_reverse_iterator(this->data(), this->data() + this->size() - 1, this->data() + this->size());
        }

        const_reverse_iterator crend() const
        {
            return const_reverse_iterator(this->data(), this->data() - 1, this->data() + this->size());
        }

        /*********************************************************************/
        /* attributes */
        /*********************************************************************/

        static size_t mult_resize_capacity;

        /*********************************************************************/
        /* utils */
        /*********************************************************************/

        template <typename NewType>
        Array<NewType> *cast() const
        {
            return dynamic_cast<Array<NewType> *>(this);
        }

        template <typename NewType>
        Array<NewType> *cast()
        {
            return dynamic_cast<Array<NewType> *>(this);
        }

        // returns a single allocated Array<MergedType> containing contiguous memory of every arrays of vector
        template <typename MergedType>
        static Array<MergedType> *merge_to_array(const std::vector<const IArray *> & arrays)
        {
            size_t total = 0;
            for (const IArray *array : arrays)
                total += array->byte_size();
            Array<MergedType> *ret = new Array<MergedType>();
            if (ret->resize(total * ret->data_size()) == false)
            {
                delete ret;
                return nullptr;
            }
            size_t copy_idx = 0;
            for (const IArray *array : arrays)
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

    protected:
        bool _internal_reserve(size_t capacity, bool clear_mem = true)
        {
            if (Array::mult_resize_capacity > 1 && _capacity > 0)
            {
                size_t new_capacity = _capacity;
                while (new_capacity < capacity)
                {
                    new_capacity = new_capacity * Array::mult_resize_capacity;
                }
                capacity = new_capacity;
            }
            _buf_ptr = (T *)realloc(_buf_ptr, capacity * sizeof(T));
            if (_buf_ptr == nullptr)
                return false;
            if (clear_mem && capacity > _capacity)
                memset(_buf_ptr + _capacity, 0, (capacity - _capacity) * sizeof(T));
            _size = std::min(_size, capacity);
            _capacity = capacity;
            _has_ownership = true;
            return true;
        }

        bool _internal_resize(size_t size, bool clear_mem = true)
        {
            if (_buf_ptr == nullptr || size > _capacity)
                this->_internal_reserve(size, clear_mem);
            if (_buf_ptr != nullptr && size <= _capacity)
                _size = size;
            return _size == size;
        }

        T *_buf_ptr;
        size_t _size;
        size_t _capacity;
        // ownership of pointer
        bool _has_ownership;
};

/*********************************************************************/
/* static attributes */
/*********************************************************************/

template <traits::TriviallyCopyable T>
size_t Array<T>::mult_resize_capacity = 2;

// typedef for types
typedef Array<bool> ArrBool;
typedef Array<char> ArrChar;
typedef Array<int8_t> ArrByte;
typedef Array<uint8_t> ArrUByte;
typedef Array<int16_t> ArrShort;
typedef Array<uint16_t> ArrUShort;
typedef Array<int32_t> ArrInt;
typedef Array<uint32_t> ArrUInt;
typedef Array<int64_t> ArrLong;
typedef Array<uint64_t> ArrULong;
typedef Array<float> ArrFloat;
typedef Array<double> ArrDouble;

template <traits::TriviallyCopyable T>
using ArrayUnique = std::unique_ptr<Array<T>>;
template <traits::TriviallyCopyable T>
using ArrayShared = std::shared_ptr<Array<T>>;
template <traits::TriviallyCopyable T>
using ArrayWeak = std::weak_ptr<Array<T>>;

template <traits::TriviallyCopyable T>
std::string_view format_as(const Array<T> & arr)
{
    return arr.cpp_str_view();
}

} // namespace sihd::util

#endif
