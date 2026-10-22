#ifndef __SIHD_UTIL_ARRAY_HPP__
#define __SIHD_UTIL_ARRAY_HPP__

#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <sihd/util/ArrayIterator.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/IArray.hpp>
#include <sihd/util/ICloneable.hpp>
#include <sihd/util/Slice.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/traits.hpp>
#include <sihd/util/type.hpp>

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

        Array(const T *data, size_t size): Array() { this->from(data, size); }

        Array(size_t capacity): Array()
        {
            if (capacity > 0)
                this->_internal_reserve(capacity);
        }

        /*********************************************************************/
        /* contiguous std containers constructors */
        /*********************************************************************/

        Array(std::string_view view)
            requires std::same_as<T, char>
            : Array(view.data(), view.size())
        {
        }

        Array(std::initializer_list<T> list): Array() { this->push_back(list.begin(), list.size()); }

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
                this->delete_buffer();
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
                this->delete_buffer();
                _buf_ptr = other._buf_ptr;
                _capacity = other._capacity;
                _size = other._size;
                _has_ownership = other._has_ownership;
                other._buf_ptr = nullptr;
                other._capacity = 0;
                other._size = 0;
                other._has_ownership = false;
            }
            return *this;
        };

        ~Array() { this->delete_buffer(); };

        /*********************************************************************/
        /* information */
        /*********************************************************************/

        operator bool() const { return _buf_ptr != nullptr; }

        operator std::string() const
            requires std::same_as<T, char>
        {
            return std::string(this->data(), this->byte_size());
        }

        operator std::string_view() const
            requires std::same_as<T, char>
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

        Type data_type() const { return type::from<T>(); }
        const char *data_type_str() const { return type::str(this->data_type()); }

        /*********************************************************************/
        /* copy_from */
        /*********************************************************************/

        bool copy_from_bytes(const void *buf, size_t buf_byte_size, Slice byte_slice = {})
        {
            auto range = byte_slice.resolve(this->byte_size());
            if (range.empty() || (range.from + buf_byte_size) > this->byte_size())
                return false;
            memmove(this->buf() + range.from, buf, buf_byte_size);
            return true;
        }

        bool copy_from_bytes(const IArray & arr, Slice byte_slice = {})
        {
            return this->copy_from_bytes(arr.buf(), arr.byte_size(), byte_slice);
        }

        bool copy_from_bytes(const IArrayView & arr, Slice byte_slice = {})
        {
            return this->copy_from_bytes(arr.buf(), arr.byte_size(), byte_slice);
        }

        /*********************************************************************/
        /* copy_to */
        /*********************************************************************/

        bool copy_to_bytes(void *buf, Slice byte_slice = {}) const
        {
            auto range = byte_slice.resolve(this->byte_size());
            if (range.empty())
                return false;
            memmove(buf, this->buf() + range.from, range.size());
            return true;
        }

        bool copy_to(Array<T> & arr, Slice slice = {}) const
        {
            auto range = slice.resolve(_size);
            if (range.empty())
                return false;
            return arr.from(_buf_ptr + range.from, range.size());
        }

        /*********************************************************************/
        /* from */
        /*********************************************************************/

        bool from_str(std::string_view data)
            requires std::same_as<T, char>
        {
            this->delete_buffer();
            return this->push_back(data);
        }

        bool from_str(std::string_view data, std::string_view delimiters)
        {
            if constexpr (std::is_fundamental_v<T>)
            {
                Splitter splitter(delimiters);
                const std::vector<std::string> splits = splitter.split(data);
                this->reserve(splits.size());
                this->resize(0);
                for (const std::string & split : splits)
                {
                    const auto value = str::convert_from_string<T>(split);
                    if (value.has_value() == false)
                        return false;
                    this->push_back(*value);
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
                    const auto byte = str::convert_from_string<uint8_t>(split, base);
                    if (byte.has_value() == false)
                        return false;
                    if (this->resize((idx / sizeof(T)) + 1) == false)
                        return false;
                    this->buf()[idx] = *byte;
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
        bool from_bytes(const void *buf, size_t buf_byte_size)
        {
            if (buf_byte_size % sizeof(T) != 0)
                throw std::invalid_argument(str::format("Array::from_bytes buffer - %lu not divisible by data size %lu",
                                                        buf_byte_size,
                                                        sizeof(T)));
            return this->from((const T *)buf, buf_byte_size / sizeof(T));
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

        void trim()
            requires std::same_as<T, char>
        {
            auto view = this->cpp_str_view();
            auto ltrim = sihd::util::str::ltrim(view);
            if (ltrim.size() < view.size())
                this->from(sihd::util::str::rtrim(ltrim));
            else
                this->resize(sihd::util::str::rtrim(ltrim).size());
        }
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
        bool assign_bytes(void *buf, size_t buf_byte_size)
        {
            return this->assign_bytes(buf, buf_byte_size, buf_byte_size);
        }

        // delete internal buffer if exists then sets it to bytes buffer buf - does not take ownership
        // throws std::invalid_argument if byte size or capacity is not aligned with type size
        bool assign_bytes(void *buf, size_t buf_byte_size, size_t buf_byte_capacity)
        {
            if (buf_byte_size % this->data_size() != 0)
                throw std::invalid_argument(str::format("Array::assign_bytes - size %lu not divisible by data size %lu",
                                                        buf_byte_size,
                                                        sizeof(T)));
            if (buf_byte_capacity % this->data_size() != 0)
                throw std::invalid_argument(
                    str::format("Array::assign_bytes - capacity %lu not divisible by data size %lu",
                                buf_byte_capacity,
                                sizeof(T)));
            return this->assign((T *)buf, buf_byte_size / this->data_size(), buf_byte_capacity / this->data_size());
        }

        /*********************************************************************/
        /* comparison */
        /*********************************************************************/

        bool is_bytes_equal(const void *buf, size_t buf_byte_size, Slice byte_slice = {}) const
        {
            auto range = byte_slice.resolve(this->byte_size());
            if ((range.from + buf_byte_size) > this->byte_size())
                return false;
            return memcmp(this->buf() + range.from, buf, buf_byte_size) == 0;
        }

        bool is_bytes_equal(const IArray & arr, Slice byte_slice = {}) const
        {
            return this->is_bytes_equal(arr.buf(), arr.byte_size(), byte_slice);
        }

        bool is_bytes_equal(const IArrayView & arr, Slice byte_slice = {}) const
        {
            return this->is_bytes_equal(arr.buf(), arr.byte_size(), byte_slice);
        }

        bool is_same_type(const IArray & arr) const { return this->data_type() == arr.data_type(); }

        bool is_same_type(const IArrayView & arr) const { return this->data_type() == arr.data_type(); }

        /*********************************************************************/
        /* views */
        /*********************************************************************/

        std::string hexdump(char delimiter = ' ') const { return str::hexdump(_buf_ptr, this->byte_size(), delimiter); }

        std::string str() const { return ArrayView<T>(*this).str(); }

        std::string str(char delimiter) const { return ArrayView<T>(*this).str(delimiter); }

        /*********************************************************************/
        /* to std containers */
        /*********************************************************************/

        std::string cpp_str() const { return ArrayView<T>(*this).cpp_str(); }

        std::string_view cpp_str_view() const { return ArrayView<T>(*this).cpp_str_view(); }

        /*********************************************************************/
        /* formatting */
        /*********************************************************************/

        std::vector<T> format_as() const { return std::vector<T>(this->data(), this->data() + this->size()); }

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

        template <class Self>
        auto data(this Self && self) -> traits::const_prop_ptr<Self, T>
        {
            return self._buf_ptr;
        }

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

        template <traits::HasDataSize Container>
        bool is_equal(const Container & container, size_t byte_offset = 0) const
        {
            return this->is_equal(container.data(), container.size(), byte_offset);
        }

        bool is_equal(std::string_view view, size_t byte_offset = 0) const
            requires std::same_as<T, char>
        {
            return this->is_equal(view.data(), view.size(), byte_offset);
        }

        bool is_equal(std::initializer_list<T> init, size_t byte_offset = 0) const
        {
            return this->is_equal(init.begin(), init.size(), byte_offset);
        }

        bool is_equal(const T *arr, size_t size, size_t byte_offset = 0) const
        {
            return this->is_bytes_equal(arr, size * this->data_size(), {(ssize_t)(byte_offset * this->data_size())});
        }

        /*********************************************************************/
        /* from */
        /*********************************************************************/

        template <traits::HasDataSize Container>
        bool from(const Container & container)
        {
            return this->from(container.data(), container.size());
        }

        bool from(std::string_view view)
            requires std::same_as<T, char>
        {
            return this->from(view.data(), view.size());
        }

        bool from(std::initializer_list<T> init) { return this->from(init.begin(), init.size()); }

        bool from(const T *arr, size_t size)
        {
            if (!_has_ownership)
                this->delete_buffer();

            if (!this->_internal_resize(size, false))
                return false;
            if (size != 0)
                memmove(_buf_ptr, arr, size * sizeof(T));
            return true;
        }

        /*********************************************************************/
        /* copy_from */
        /*********************************************************************/

        template <traits::HasDataSize Container>
        bool copy_from(const Container & container, size_t byte_offset = 0)
        {
            return this->copy_from(container.data(), container.size(), byte_offset);
        }

        bool copy_from(std::string_view view, size_t byte_offset = 0)
            requires std::same_as<T, char>
        {
            return this->copy_from(view.data(), view.size(), byte_offset);
        }

        bool copy_from(std::initializer_list<T> init, size_t byte_offset = 0)
        {
            return this->copy_from(init.begin(), init.size(), byte_offset);
        }

        bool copy_from(const T *arr, size_t size, size_t byte_offset = 0)
        {
            return this->copy_from_bytes(arr, size * this->data_size(), {(ssize_t)(byte_offset * this->data_size())});
        }

        /*********************************************************************/
        /* assign */
        /*********************************************************************/

        // delete internal buffer if exists then sets it to container buf - does not take ownership
        template <traits::HasDataSize Container>
        bool assign(Container & container)
        {
            return this->assign(container.data(), container.size());
        }

        bool assign(char *str)
            requires std::same_as<T, char>
        {
            size_t size = str != nullptr ? strlen(str) : 0;
            return this->assign(str, size, size);
        }

        bool assign(std::string & str)
            requires std::same_as<T, char>
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

        bool push_back(std::string_view str)
            requires std::same_as<T, char>
        {
            return this->push_back(str.data(), str.size());
        }

        bool push_back(std::initializer_list<T> init) { return this->push_back(init.begin(), init.size()); }

        bool push_back(const T *buf, size_t size) { return this->insert(buf, size, _size); }

        bool push_back(const T & value) { return this->insert(value, _size); }

        /*********************************************************************/
        /* push_front */
        /*********************************************************************/

        template <traits::HasDataSize Container>
        bool push_front(const Container & container)
        {
            return this->push_front(container.data(), container.size());
        }

        bool push_front(std::string_view str)
            requires std::same_as<T, char>
        {
            return this->push_front(str.data(), str.size());
        }

        bool push_front(std::initializer_list<T> init) { return this->push_front(init.begin(), init.size()); }

        bool push_front(const T *buf, size_t size) { return this->insert(buf, size, 0); }

        bool push_front(const T & value) { return this->insert(value, 0); }

        /*********************************************************************/
        /* modify */
        /*********************************************************************/

        bool insert(const T *buf, size_t size, size_t idx)
        {
            if (idx > _size)
                return false;
            if (size == 0)
                return true;
            if (size > std::numeric_limits<size_t>::max() / sizeof(T) - _size)
                return false;
            const std::less<const T *> less;
            if (_buf_ptr != nullptr && less(buf, _buf_ptr + _size) && less(_buf_ptr, buf + size))
            {
                Array<T> copy(buf, size);
                return copy.size() == size && this->insert(copy.data(), size, idx);
            }
            if (_size + size > _capacity && !this->_internal_reserve(_size + size, false))
                return false;
            if (_buf_ptr != nullptr)
            {
                size_t len_move = (_size - idx) * this->data_size();
                if (len_move > 0)
                {
                    memmove((uint8_t *)(_buf_ptr + (idx + size)), (uint8_t *)(_buf_ptr + idx), len_move);
                }
                memmove((uint8_t *)(_buf_ptr + idx), buf, size * this->data_size());
                _size += size;
            }
            return _buf_ptr != nullptr;
        }

        bool insert(const T & value, size_t idx) { return this->insert(&value, 1, idx); }

        void pop(size_t idx)
        {
            if (idx >= _size)
                throw std::out_of_range("Array::pop: index exceeds size");

            size_t len = (_size - (idx + 1)) * this->data_size();

            memmove((uint8_t *)(_buf_ptr + idx), (uint8_t *)(_buf_ptr + idx + 1), len);

            memset((uint8_t *)(_buf_ptr + _size - 1), 0, this->data_size());
            --_size;
        }

        void pop_back() { pop(_size - 1); }

        void pop_front() { pop(0); }

        // set value at idx - throws out_of_range
        void set(size_t idx, const T & value)
        {
            if (idx >= _size)
                throw std::out_of_range("Array::set: index exceeds size");
            _buf_ptr[idx] = value;
        }

        [[nodiscard]] T & get(size_t idx)
        {
            if (idx >= _size)
                throw std::out_of_range("Array::get: index exceeds size");
            return _buf_ptr[idx];
        }

        [[nodiscard]] const T & get(size_t idx) const
        {
            if (idx >= _size)
                throw std::out_of_range("Array::get: index exceeds size");
            return _buf_ptr[idx];
        }

        /*********************************************************************/
        /* access */
        /*********************************************************************/

        [[nodiscard]] T front() const { return this->at(0); }

        [[nodiscard]] T back() const { return this->at(_size - 1); }

        // access value at idx - throws out_of_range
        [[nodiscard]] T at(size_t idx) const
        {
            if (idx >= _size)
                throw std::out_of_range("Array::at: index exceeds size");
            return _buf_ptr[idx];
        }

        T operator[](size_t idx) const { return _buf_ptr[idx]; }

        T & operator[](size_t idx) { return _buf_ptr[idx]; }

        /*********************************************************************/
        /* iterator */
        /*********************************************************************/

        using iterator = ArrayIterator<T>;
        using const_iterator = ArrayIterator<const T>;

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

        using reverse_iterator = ReverseArrayIterator<T>;
        using const_reverse_iterator = ReverseArrayIterator<const T>;

        reverse_iterator rbegin()
        {
            if (_size == 0)
                return this->rend();
            return reverse_iterator(this->data(), this->data() + this->size() - 1, this->data() + this->size());
        }
        reverse_iterator rend()
        {
            if (this->data() == nullptr)
                return reverse_iterator();
            return reverse_iterator(this->data(), this->data() - 1, this->data() + this->size());
        }

        const_reverse_iterator crbegin() const
        {
            if (_size == 0)
                return this->crend();
            return const_reverse_iterator(this->data(), this->data() + this->size() - 1, this->data() + this->size());
        }

        const_reverse_iterator crend() const
        {
            if (this->data() == nullptr)
                return const_reverse_iterator();
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
            if (ret->byte_resize(total) == false)
            {
                delete ret;
                return nullptr;
            }
            size_t copy_idx = 0;
            for (const IArray *array : arrays)
            {
                if (ret->copy_from_bytes(*array, {(ssize_t)copy_idx}) == false)
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
            const size_t max_capacity = std::numeric_limits<size_t>::max() / sizeof(T);
            if (capacity > max_capacity)
                return false;
            if (capacity == 0)
                return true;
            if (Array::mult_resize_capacity > 1 && _capacity > 0)
            {
                size_t new_capacity = _capacity;
                while (new_capacity < capacity && new_capacity <= max_capacity / Array::mult_resize_capacity)
                {
                    new_capacity = new_capacity * Array::mult_resize_capacity;
                }
                capacity = std::max(capacity, new_capacity);
            }

            T *new_ptr = _has_ownership ? (T *)realloc(_buf_ptr, capacity * sizeof(T))
                                        : (T *)malloc(capacity * sizeof(T));
            if (new_ptr == nullptr)
                return false;
            if (!_has_ownership && _buf_ptr != nullptr)
                memcpy(new_ptr, _buf_ptr, std::min(_capacity, capacity) * sizeof(T));
            _buf_ptr = new_ptr;

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

static_assert(std::input_iterator<Array<int>::iterator>, "failed input iterator");
static_assert(std::output_iterator<Array<int>::iterator, int>, "failed output iterator");
static_assert(std::forward_iterator<Array<int>::iterator>, "failed forward iterator");
static_assert(std::input_iterator<Array<int>::iterator>, "failed input iterator");
static_assert(std::bidirectional_iterator<Array<int>::iterator>, "failed bidirectional iterator");
static_assert(std::contiguous_iterator<Array<int>::iterator>, "failed random access iterator");
static_assert(std::weakly_incrementable<Array<int>::iterator>, "Failed the weakly incrementable test");
static_assert(std::movable<Array<int>::iterator>, "Failed the moveable test");
static_assert(std::default_initializable<Array<int>::iterator>, "Failed the default initializable test");

static_assert(std::input_iterator<Array<int>::reverse_iterator>, "failed input iterator");
static_assert(std::output_iterator<Array<int>::reverse_iterator, int>, "failed output iterator");
static_assert(std::forward_iterator<Array<int>::reverse_iterator>, "failed forward iterator");
static_assert(std::input_iterator<Array<int>::reverse_iterator>, "failed input iterator");
static_assert(std::bidirectional_iterator<Array<int>::reverse_iterator>, "failed bidirectional iterator");
static_assert(std::contiguous_iterator<Array<int>::reverse_iterator>, "failed random access iterator");
static_assert(std::weakly_incrementable<Array<int>::reverse_iterator>, "Failed the weakly incrementable test");
static_assert(std::movable<Array<int>::reverse_iterator>, "Failed the moveable test");
static_assert(std::default_initializable<Array<int>::reverse_iterator>, "Failed the default initializable test");

using ArrBool = Array<bool>;
using ArrChar = Array<char>;
using ArrByte = Array<int8_t>;
using ArrUByte = Array<uint8_t>;
using ArrShort = Array<int16_t>;
using ArrUShort = Array<uint16_t>;
using ArrInt = Array<int32_t>;
using ArrUInt = Array<uint32_t>;
using ArrLong = Array<int64_t>;
using ArrULong = Array<uint64_t>;
using ArrFloat = Array<float>;
using ArrDouble = Array<double>;

template <traits::TriviallyCopyable T>
using ArrayUnique = std::unique_ptr<Array<T>>;
template <traits::TriviallyCopyable T>
using ArrayShared = std::shared_ptr<Array<T>>;
template <traits::TriviallyCopyable T>
using ArrayWeak = std::weak_ptr<Array<T>>;

} // namespace sihd::util

#endif
