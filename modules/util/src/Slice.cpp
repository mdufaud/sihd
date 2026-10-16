#include <algorithm>

#include <sihd/util/Slice.hpp>

namespace sihd::util
{

size_t Slice::Range::size() const
{
    return to - from;
}

bool Slice::Range::empty() const
{
    return from >= to;
}

Slice::Range::operator bool() const
{
    return !this->empty();
}

Slice Slice::from_size(size_t offset, size_t size)
{
    if (size == 0)
        return {1, 0};
    return {static_cast<ssize_t>(offset), static_cast<ssize_t>(offset + size - 1)};
}

Slice::Range Slice::resolve(size_t size) const
{
    const ssize_t sz = static_cast<ssize_t>(size);
    ssize_t f = from < 0 ? from + sz : from;
    ssize_t t = to < 0 ? to + sz : to;
    f = std::clamp(f, ssize_t(0), sz);
    t = std::clamp(t, ssize_t(-1), sz - 1);
    if (f > t)
        return {0, 0};
    return {static_cast<size_t>(f), static_cast<size_t>(t + 1)};
}

} // namespace sihd::util
