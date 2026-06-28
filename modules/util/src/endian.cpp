#include <bit>

#include <sihd/util/endian.hpp>

namespace sihd::util::endian
{

namespace
{

constexpr uint16_t bswap(uint16_t x) { return __builtin_bswap16(x); }
constexpr uint32_t bswap(uint32_t x) { return __builtin_bswap32(x); }
constexpr uint64_t bswap(uint64_t x) { return __builtin_bswap64(x); }
constexpr int16_t bswap(int16_t x) { return (int16_t)__builtin_bswap16((uint16_t)x); }
constexpr int32_t bswap(int32_t x) { return (int32_t)__builtin_bswap32((uint32_t)x); }
constexpr int64_t bswap(int64_t x) { return (int64_t)__builtin_bswap64((uint64_t)x); }

template <typename T>
constexpr T to_big(T x)
{
    if constexpr (std::endian::native == std::endian::little)
        return bswap(x);
    else
        return x;
}

template <typename T>
constexpr T to_little(T x)
{
    if constexpr (std::endian::native == std::endian::big)
        return bswap(x);
    else
        return x;
}

} // namespace

std::string type_str(std::endian type)
{
    switch (type)
    {
        case std::endian::big:
            return "big";
        case std::endian::little:
            return "little";
        default:
            return "unknown";
    }
}

bool switch_buffer_endianness(void *buf, ByteSize size, size_t buf_size)
{
    size_t i = 0;
    switch (size)
    {
        case ByteSize::b2:
        {
            auto *buf2 = static_cast<int16_t *>(buf);
            while (i < buf_size)
            {
                buf2[i] = bswap(buf2[i]);
                ++i;
            }
            break;
        }
        case ByteSize::b4:
        {
            auto *buf4 = static_cast<int32_t *>(buf);
            while (i < buf_size)
            {
                buf4[i] = bswap(buf4[i]);
                ++i;
            }
            break;
        }
        case ByteSize::b8:
        {
            auto *buf8 = static_cast<int64_t *>(buf);
            while (i < buf_size)
            {
                buf8[i] = bswap(buf8[i]);
                ++i;
            }
            break;
        }
        default:
            return false;
    }
    return true;
}

template <>
uint16_t swap<uint16_t>(uint16_t ret) { return bswap(ret); }
template <>
int16_t swap<int16_t>(int16_t ret) { return bswap(ret); }
template <>
uint32_t swap<uint32_t>(uint32_t ret) { return bswap(ret); }
template <>
int32_t swap<int32_t>(int32_t ret) { return bswap(ret); }
template <>
uint64_t swap<uint64_t>(uint64_t ret) { return bswap(ret); }
template <>
int64_t swap<int64_t>(int64_t ret) { return bswap(ret); }

template <>
float swap<float>(float ret)
{
    uint32_t tmp;
    __builtin_memcpy(&tmp, &ret, sizeof(tmp));
    tmp = bswap(tmp);
    __builtin_memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

template <>
double swap<double>(double ret)
{
    uint64_t tmp;
    __builtin_memcpy(&tmp, &ret, sizeof(tmp));
    tmp = bswap(tmp);
    __builtin_memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

template <>
uint16_t convert<uint16_t, std::endian::little>(uint16_t value) { return to_little(value); }
template <>
uint16_t convert<uint16_t, std::endian::big>(uint16_t value) { return to_big(value); }
template <>
int16_t convert<int16_t, std::endian::little>(int16_t value) { return to_little(value); }
template <>
int16_t convert<int16_t, std::endian::big>(int16_t value) { return to_big(value); }

template <>
uint32_t convert<uint32_t, std::endian::little>(uint32_t value) { return to_little(value); }
template <>
uint32_t convert<uint32_t, std::endian::big>(uint32_t value) { return to_big(value); }
template <>
int32_t convert<int32_t, std::endian::little>(int32_t value) { return to_little(value); }
template <>
int32_t convert<int32_t, std::endian::big>(int32_t value) { return to_big(value); }

template <>
uint64_t convert<uint64_t, std::endian::little>(uint64_t value) { return to_little(value); }
template <>
uint64_t convert<uint64_t, std::endian::big>(uint64_t value) { return to_big(value); }
template <>
int64_t convert<int64_t, std::endian::little>(int64_t value) { return to_little(value); }
template <>
int64_t convert<int64_t, std::endian::big>(int64_t value) { return to_big(value); }

template <>
uint16_t convert_from<uint16_t, std::endian::little>(uint16_t value) { return to_little(value); }
template <>
uint16_t convert_from<uint16_t, std::endian::big>(uint16_t value) { return to_big(value); }
template <>
int16_t convert_from<int16_t, std::endian::little>(int16_t value) { return to_little(value); }
template <>
int16_t convert_from<int16_t, std::endian::big>(int16_t value) { return to_big(value); }

template <>
uint32_t convert_from<uint32_t, std::endian::little>(uint32_t value) { return to_little(value); }
template <>
uint32_t convert_from<uint32_t, std::endian::big>(uint32_t value) { return to_big(value); }
template <>
int32_t convert_from<int32_t, std::endian::little>(int32_t value) { return to_little(value); }
template <>
int32_t convert_from<int32_t, std::endian::big>(int32_t value) { return to_big(value); }

template <>
uint64_t convert_from<uint64_t, std::endian::little>(uint64_t value) { return to_little(value); }
template <>
uint64_t convert_from<uint64_t, std::endian::big>(uint64_t value) { return to_big(value); }
template <>
int64_t convert_from<int64_t, std::endian::little>(int64_t value) { return to_little(value); }
template <>
int64_t convert_from<int64_t, std::endian::big>(int64_t value) { return to_big(value); }

} // namespace sihd::util::endian
