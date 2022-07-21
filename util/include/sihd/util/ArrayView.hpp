#ifndef __SIHD_UTIL_ARRAYVIEW_HPP__
# define __SIHD_UTIL_ARRAYVIEW_HPP__

# include <string.h>
# include <strings.h>

# include <utility>
# include <cstdint>
# include <stdexcept>
# include <memory>
# include <algorithm>

# include <sihd/util/IArrayView.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/Array.hpp>

namespace sihd::util
{

SIHD_LOGGER;

template <typename T>
class ArrayView: public IArrayView
{
    public:
        using value_type = T;
        using size_type = size_t;
        using pointer = T *;
        using const_pointer = const T *;
        using reference = T &;
        using const_reference = const T &;

        static constexpr size_t npos = Array<T>::npos;

        static_assert(std::is_trivially_copyable_v<T>);

        ArrayView(const T *data, size_t size): _buf_ptr(data), _size(size) {}
        ArrayView(const std::vector<T> & vec): ArrayView(vec.data(), vec.size()) {}
        ArrayView(const Array<T> & arr): ArrayView(arr.data(), arr.size()) {}
        ArrayView(const ArrayView<T> & arr): ArrayView(arr.data(), arr.size()) {}

        // remove conversion with temporary allocation
        ArrayView(const std::vector<T> && vec) = delete;
        // remove conversion with temporary allocation
        ArrayView(const Array<T> && arr) = delete;

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        ArrayView(const char *str): ArrayView(str, strlen(str)) {}

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        ArrayView(std::string_view view): ArrayView(view.data(), view.size()) {}

        // char specialization
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        ArrayView(const std::string & str): ArrayView(str.data(), str.size()) {}

        // char specialization
        // remove conversion with temporary allocation
        template <typename Char = T, std::enable_if_t<std::is_same_v<Char, char>, char> = 0>
        ArrayView(std::string && str) = delete;

        /*********************************************************************/
        /* byte constructor */
        /*********************************************************************/

        // make sure IArrayView size is divisible by type size.
        // ex: ArrayView<int8_t> of size 3 may not go into an ArrayView<int32_t> which will be size 0
        ArrayView(const IArrayView & arr): ArrayView((const T *)arr.buf(), arr.byte_size() / sizeof(T)) {}

        // make sure IArray size is divisible by type size.
        // ex: Array<int8_t> of size 3 may not go into an ArrayView<int32_t> which will be size 0
        ArrayView(const IArray & arr): ArrayView((const T *)arr.buf(), arr.byte_size() / sizeof(T)) {}

        // make sure buffer size is divisible by type size.
        // ex: buffer[3] may not go into an ArrayView<int32_t> which will be size 0
        ArrayView(const void *data, size_t byte_size): ArrayView((const T *)data, byte_size) {}

        // WARNING: using braces initializer must only be used passed in functions,
        // where scope allows std::initializer_list's buffer to live
        ArrayView(std::initializer_list<T> init): ArrayView(&(*init.begin()), init.size()) {}

        /*********************************************************************/
        /* copy constructor */
        /*********************************************************************/

        ArrayView(ArrayView<T> && other)
        {
            *this = std::move(other);
        }

        /*********************************************************************/
        /* operator= */
        /*********************************************************************/

        ArrayView & operator=(const ArrayView<T> & other)
        {
            if (&other != this)
            {
                _buf_ptr = other._buf_ptr;
                _size = other._size;
            }
            return *this;
        }

        ArrayView & operator=(ArrayView<T> && other)
        {
            if (&other != this)
            {
                _buf_ptr = other._buf_ptr;
                _size = other._size;
                other._buf_ptr = nullptr;
                other._size = 0;
            }
            return *this;
        };

        virtual ~ArrayView() {};

        /*********************************************************************/
        /* information */
        /*********************************************************************/

        operator bool() const { return _buf_ptr != nullptr; }

        const uint8_t *buf() const { return (uint8_t *)_buf_ptr; }

        size_t byte_index(size_t index) const { return index * sizeof(T); }
        const uint8_t *buf_at(size_t index) const { return (uint8_t *)&_buf_ptr[index]; }

        size_t data_size() const { return sizeof(T); }

        size_t size() const { return _size; }
        size_t byte_size() const { return _size * sizeof(T); }
        bool empty() const { return _size == 0; }

        Type data_type() const { return Types::type<T>(); }

        const char *data_type_str() const { return Types::type_str(this->data_type()); }

        /*********************************************************************/
        /* comparison */
        /*********************************************************************/

        bool is_same_type(const IArrayView & arr) const
        {
            return this->data_type() == arr.data_type();
        }

        bool is_bytes_equal(const void *buf, size_t size, size_t byte_offset = 0) const
        {
            if (byte_offset > this->byte_size())
                return false;
            if ((this->byte_size() - byte_offset) < size)
                return false;
            return memcmp(this->buf() + byte_offset, buf, size) == 0;
        }

        bool is_bytes_equal(const IArrayView & arr, size_t byte_offset = 0) const
        {
            return this->is_bytes_equal(arr.buf(), arr.byte_size(), byte_offset);
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
        /* views */
        /*********************************************************************/

        std::string hexdump(char delimiter = ' ') const
        {
            return Str::hexdump(_buf_ptr, this->byte_size(), delimiter);
        }

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
                return Str::hexdump(_buf_ptr, this->byte_size(), 0);
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
                return Str::hexdump(_buf_ptr, this->byte_size(), delimiter);
        }

        /*********************************************************************/
        /* to std containers */
        /*********************************************************************/

        std::string cpp_str() const
        {
            return _buf_ptr != nullptr ? std::string((char *)_buf_ptr, this->byte_size()) : "";
        }

        std::string_view cpp_str_view() const
        {
            return _buf_ptr != nullptr ? std::string_view((char *)_buf_ptr, this->byte_size()) : "";
        }

        /*********************************************************************/
        /* class methods */
        /*********************************************************************/

        const T *data() const { return _buf_ptr; }

        /*********************************************************************/
        /* to std containers */
        /*********************************************************************/

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

        /*********************************************************************/
        /* view change */
        /*********************************************************************/

        ArrayView<T> & remove_prefix(size_t size)
        {
            if (size > _size)
                size = _size;
            _buf_ptr += size;
            _size -= size;
            return *this;
        }

        ArrayView<T> & remove_suffix(size_t size)
        {
            if (size > _size)
                size = _size;
            _size -= size;
            return *this;
        }

        // "hello world".subview(6) -> "world"
        // "hello world".subview(0, 5) -> "hello"
        ArrayView<T> subview(size_t pos, size_t count = -1)
        {
            pos = std::min(pos, _size);
            count = std::min(count, _size);
            count = std::min(count - pos, _size);
            return ArrayView<T>(this->data() + pos, count);
        }

        /*********************************************************************/
        /* is_equal */
        /*********************************************************************/

        bool is_equal(ArrayView<T> arr, size_t offset = 0) const
        {
            return this->is_equal(arr.data(), arr.size(), offset);
        }

        // compares memory from internal buffer and array of size
        bool is_equal(const T *arr, size_t size, size_t offset = 0) const
        {
            return this->is_bytes_equal(arr, size * this->data_size(), offset * this->data_size());
        }

        /*********************************************************************/
        /* copy_to */
        /*********************************************************************/

        bool copy_to(T *arr, size_t size, size_t offset = 0) const
        {
            return this->copy_to_bytes(arr, size * this->data_size(), offset * this->data_size());
        }

        /*********************************************************************/
        /* access */
        /*********************************************************************/

        // data first value
        inline T front() const { return _buf_ptr[0]; }

        // access last value
        inline T back() const { return _buf_ptr[_size - 1]; }

        // access value at idx - throws out_of_range
        T at(size_t idx) const
        {
            if (idx >= _size)
                throw std::out_of_range("ArrayView::at: index exceeds size");
            return _buf_ptr[idx];
        }

        // access value arr[idx]
        inline T operator[](size_t idx) const { return _buf_ptr[idx]; }

        /*********************************************************************/
        /* iterator */
        /*********************************************************************/
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
                    if (this->array_curr < this->array_beg && this->array_curr > this->array_end)
                        throw std::out_of_range("Array::iterator: iterator out of range");
                    return *this->array_curr;
                }

                size_t idx()
                {
                    if (this->array_curr < this->array_beg && this->array_curr > this->array_end)
                        return ArrayView<T>::npos;
                    return this->array_curr - this->array_beg;
                }

        };

        typedef ArrayIterator<const T> iterator;

        iterator begin() const { return iterator(this->data(), this->data(), this->data() + this->size()); }
        iterator end() const { return iterator(this->data(), this->data() + this->size(), this->data() + this->size()); }

        /*********************************************************************/
        /* reverse iterator */
        /*********************************************************************/

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
                    if (this->array_curr < this->array_beg && this->array_curr > this->array_end)
                        throw std::out_of_range("Array::reverse_iterator: iterator out of range");
                    return *this->array_curr;
                }

                size_t idx()
                {
                    if (this->array_curr < this->array_beg && this->array_curr > this->array_end)
                        return ArrayView<T>::npos;
                    return this->array_curr - this->array_beg;
                }
        };

        typedef ReverseArrayIterator<const T> reverse_iterator;

        reverse_iterator rbegin() const
        {
            return reverse_iterator(this->data(),
                                    this->data() + this->size() - 1,
                                    this->data() + this->size());
        }
        reverse_iterator rend() const
        {
            return reverse_iterator(this->data(),
                                    this->data() - 1,
                                    this->data() + this->size());
        }

        /*********************************************************************/
        /* iterator operations */
        /*********************************************************************/

        size_t find(const T value) const
        {
            return std::find(this->begin(), this->end(), value).idx();
        }

        size_t rfind(const T value) const
        {
            return std::find(this->rbegin(), this->rend(), value).idx();
        }

    /*********************************************************************/
    /* attributes */
    /*********************************************************************/

    protected:
        const T *_buf_ptr;
        size_t _size;
};

// typedef for types
typedef ArrayView<bool>       ArrViewBool;
typedef ArrayView<char>       ArrViewChar;
typedef ArrayView<int8_t>     ArrViewByte;
typedef ArrayView<uint8_t>    ArrViewUByte;
typedef ArrayView<int16_t>    ArrViewShort;
typedef ArrayView<uint16_t>   ArrViewUShort;
typedef ArrayView<int32_t>    ArrViewInt;
typedef ArrayView<uint32_t>   ArrViewUInt;
typedef ArrayView<int64_t>    ArrViewLong;
typedef ArrayView<uint64_t>   ArrViewULong;
typedef ArrayView<float>      ArrViewFloat;
typedef ArrayView<double>     ArrViewDouble;

// template class ArrayView<bool>;
// extern template class ArrayView<bool>;

template class ArrayView<int8_t>;
extern template class ArrayView<int8_t>;

template class ArrayView<char>;
extern template class ArrayView<char>;

template class ArrayView<int>;
extern template class ArrayView<int>;

}

#endif