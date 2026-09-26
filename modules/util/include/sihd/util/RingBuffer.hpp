#ifndef __SIHD_UTIL_RINGBUFFER_HPP__
#define __SIHD_UTIL_RINGBUFFER_HPP__

#include <algorithm>
#include <cstddef>
#include <span>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>

namespace sihd::util
{

// fixed capacity ring, single threaded - read_span/write_span expose at most one contiguous
// chunk, read_span is read-only (ArrayView holds const pointers)
template <typename T>
class RingBuffer
{
    public:
        RingBuffer(): RingBuffer(0) {}

        explicit RingBuffer(size_t capacity): _buffer(capacity) {}

        size_t capacity() const { return _buffer.capacity(); }
        size_t size() const { return _size; }
        size_t available() const { return this->capacity() - _size; }
        bool empty() const { return _size == 0; }
        bool full() const { return _size == this->capacity(); }

        void clear()
        {
            _head = 0;
            _size = 0;
            _writable_span_size = 0;
        }

        size_t write(const T *data, size_t size)
        {
            const size_t n = std::min(size, this->available());
            if (n == 0)
                return 0;
            const size_t tail = (_head + _size) % this->capacity();
            const size_t first = std::min(n, this->capacity() - tail);
            std::copy_n(data, first, _buffer.data() + tail);
            std::copy_n(data + first, n - first, _buffer.data());
            _size += n;
            _writable_span_size = 0;
            return n;
        }

        size_t write(ArrayView<T> data) { return this->write(data.data(), data.size()); }

        size_t read(T *out, size_t size)
        {
            const size_t n = this->_copy_out(out, size);
            if (n == 0)
                return 0;
            _head = (_head + n) % this->capacity();
            _size -= n;
            return n;
        }

        size_t peek(T *out, size_t size) const { return this->_copy_out(out, size); }

        size_t skip(size_t size)
        {
            const size_t n = std::min(size, _size);
            if (n == 0)
                return 0;
            _head = (_head + n) % this->capacity();
            _size -= n;
            return n;
        }

        ArrayView<T> read_span() const
        {
            const size_t n = std::min(_size, this->capacity() - _head);
            return ArrayView<T>(_buffer.data() + _head, n);
        }

        // fill write_span() then produce what was written
        std::span<T> write_span()
        {
            const size_t free = this->available();
            if (free == 0)
            {
                _writable_span_size = 0;
                return {};
            }
            const size_t tail = (_head + _size) % this->capacity();
            _writable_span_size = std::min(free, this->capacity() - tail);
            return std::span<T>(_buffer.data() + tail, _writable_span_size);
        }

        size_t produce(size_t size)
        {
            const size_t n = std::min(size, _writable_span_size);
            _size += n;
            _writable_span_size = 0;
            return n;
        }

    private:
        size_t _copy_out(T *out, size_t size) const
        {
            const size_t n = std::min(size, _size);
            if (n == 0)
                return 0;
            const size_t first = std::min(n, this->capacity() - _head);
            std::copy_n(_buffer.data() + _head, first, out);
            std::copy_n(_buffer.data(), n - first, out + first);
            return n;
        }

        Array<T> _buffer;
        size_t _head = 0;
        size_t _size = 0;
        size_t _writable_span_size = 0;
};

} // namespace sihd::util

#endif
