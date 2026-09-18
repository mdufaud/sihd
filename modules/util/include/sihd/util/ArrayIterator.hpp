#ifndef __SIHD_UTIL_ARRAYITERATOR_HPP__
#define __SIHD_UTIL_ARRAYITERATOR_HPP__

#include <cstddef>   // std::ptrdiff_t, size_t
#include <iterator>  // std::contiguous_iterator_tag
#include <stdexcept> // out of range

namespace sihd::util
{

// iterator over a contiguous range of pointers, walks forward
template <typename IteratorType>
class ArrayIterator
{
    public:
        // Iterator traits - typedefs and types required to be STL compliant
        using iterator_category = std::contiguous_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = IteratorType;
        using pointer = IteratorType *;
        using reference = IteratorType &;

        static constexpr size_t npos = size_t(-1);

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
            ArrayIterator ret(*this);
            ++(*this);
            return ret;
        }

        ArrayIterator operator--(int)
        {
            ArrayIterator ret(*this);
            --(*this);
            return ret;
        }

        ArrayIterator & operator+=(const difference_type ptr_diff)
        {
            this->array_curr += ptr_diff;
            return *this;
        }

        ArrayIterator & operator-=(const difference_type ptr_diff)
        {
            this->array_curr -= ptr_diff;
            return *this;
        }

        difference_type operator-(const ArrayIterator & rhs) const { return this->array_curr - rhs.array_curr; }

        ArrayIterator operator+(difference_type i) const
        {
            return ArrayIterator(this->array_beg, this->array_curr + i, this->array_end);
        }

        ArrayIterator operator-(difference_type i) const
        {
            return ArrayIterator(this->array_beg, this->array_curr - i, this->array_end);
        }

        bool operator==(const ArrayIterator & rhs) const { return this->array_curr == rhs.array_curr; }
        bool operator!=(const ArrayIterator & rhs) const { return !(*this == rhs); }

        bool operator<(const ArrayIterator & rhs) const { return this->array_curr < rhs.array_curr; }
        bool operator>(const ArrayIterator & rhs) const { return !(*this <= rhs); }
        bool operator<=(const ArrayIterator & rhs) const { return this->array_curr <= rhs.array_curr; }
        bool operator>=(const ArrayIterator & rhs) const { return !(*this < rhs); }

        reference operator*() const
        {
            if (this->array_curr < this->array_beg || this->array_curr >= this->array_end)
                throw std::out_of_range("Array::iterator: iterator out of range");
            return *this->array_curr;
        }

        pointer operator->() const { return this->array_curr; }

        reference operator[](difference_type n) const { return *(this->array_curr + n); }

        size_t idx() const
        {
            if (this->array_curr < this->array_beg || this->array_curr >= this->array_end)
                return npos;
            return this->array_curr - this->array_beg;
        }
};

// iterator over a contiguous range of pointers, walks backward
template <typename IteratorType>
class ReverseArrayIterator
{
    public:
        // Iterator traits - typedefs and types required to be STL compliant
        using iterator_category = std::contiguous_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = IteratorType;
        using pointer = IteratorType *;
        using reference = IteratorType &;

        static constexpr size_t npos = size_t(-1);

        pointer array_beg;
        pointer array_curr;
        pointer array_end;

        ReverseArrayIterator(pointer ptr_begin = nullptr, pointer ptr_curr = nullptr, pointer ptr_end = nullptr):
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
            ReverseArrayIterator ret(*this);
            ++(*this);
            return ret;
        }

        ReverseArrayIterator operator--(int)
        {
            ReverseArrayIterator ret(*this);
            --(*this);
            return ret;
        }

        difference_type operator-(const ReverseArrayIterator & rhs) const { return rhs.array_curr - this->array_curr; }

        ReverseArrayIterator operator+(difference_type i) const
        {
            return ReverseArrayIterator(this->array_beg, this->array_curr - i, this->array_end);
        }

        ReverseArrayIterator operator-(difference_type i) const
        {
            return ReverseArrayIterator(this->array_beg, this->array_curr + i, this->array_end);
        }

        ReverseArrayIterator & operator+=(const difference_type ptr_diff)
        {
            this->array_curr -= ptr_diff;
            return *this;
        }

        ReverseArrayIterator & operator-=(const difference_type ptr_diff)
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

        reference operator*() const
        {
            if (this->array_curr < this->array_beg || this->array_curr >= this->array_end)
                throw std::out_of_range("Array::reverse_iterator: iterator out of range");
            return *this->array_curr;
        }

        pointer operator->() const { return this->array_curr; }

        reference operator[](difference_type n) const { return *(this->array_curr + n); }

        size_t idx() const
        {
            if (this->array_curr < this->array_beg || this->array_curr >= this->array_end)
                return npos;
            return this->array_curr - this->array_beg;
        }
};

template <typename IteratorType>
ArrayIterator<IteratorType> operator+(typename ArrayIterator<IteratorType>::difference_type i,
                                      const ArrayIterator<IteratorType> & it)
{
    return ArrayIterator<IteratorType>(it.array_beg, it.array_curr + i, it.array_end);
}

template <typename IteratorType>
ReverseArrayIterator<IteratorType> operator+(typename ReverseArrayIterator<IteratorType>::difference_type i,
                                             const ReverseArrayIterator<IteratorType> & it)
{
    return ReverseArrayIterator<IteratorType>(it.array_beg, it.array_curr - i, it.array_end);
}
} // namespace sihd::util

#endif
