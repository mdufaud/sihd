#include <sihd/util/endian.hpp>

namespace sihd::util::endian
{

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
    int16_t *buf2 = nullptr;
    int32_t *buf4 = nullptr;
    int64_t *buf8 = nullptr;
    size_t i = 0;
    switch (size)
    {
        case ByteSize::b2:
        {
            buf2 = static_cast<int16_t *>(buf);
            while (i < buf_size)
            {
                buf2[i] = bswap_16(buf2[i]);
                ++i;
            }
            break;
        }
        case ByteSize::b4:
        {
            buf4 = static_cast<int32_t *>(buf);
            while (i < buf_size)
            {
                buf4[i] = bswap_32(buf4[i]);
                ++i;
            }
            break;
        }
        case ByteSize::b8:
        {
            buf8 = static_cast<int64_t *>(buf);
            while (i < buf_size)
            {
                buf8[i] = bswap_64(buf8[i]);
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
uint16_t swap<uint16_t>(uint16_t ret)
{
    return bswap_16(ret);
};
template <>
int16_t swap<int16_t>(int16_t ret)
{
    return bswap_16(ret);
};
template <>
uint32_t swap<uint32_t>(uint32_t ret)
{
    return bswap_32(ret);
};
template <>
int32_t swap<int32_t>(int32_t ret)
{
    return bswap_32(ret);
};
template <>
uint64_t swap<uint64_t>(uint64_t ret)
{
    return bswap_64(ret);
};
template <>
int64_t swap<int64_t>(int64_t ret)
{
    return bswap_64(ret);
};

template <>
float swap<float>(float ret)
{
    float *ret_p = &ret;
    uint32_t *tmp = reinterpret_cast<uint32_t *>(ret_p);
    uint32_t swapped = bswap_32(*tmp);
    tmp = &swapped;
    return *(reinterpret_cast<float *>(tmp));
}

template <>
double swap<double>(double ret)
{
    double *ret_p = &ret;
    uint64_t *tmp = reinterpret_cast<uint64_t *>(ret_p);
    uint64_t swapped = bswap_64(*tmp);
    tmp = &swapped;
    return *(reinterpret_cast<double *>(tmp));
}

template <>
uint16_t convert<uint16_t, std::endian::little>(uint16_t value)
{
    return le16toh(value);
}
template <>
uint16_t convert<uint16_t, std::endian::big>(uint16_t value)
{
    return be16toh(value);
}
template <>
int16_t convert<int16_t, std::endian::little>(int16_t value)
{
    return le16toh(value);
}
template <>
int16_t convert<int16_t, std::endian::big>(int16_t value)
{
    return be16toh(value);
}

template <>
uint32_t convert<uint32_t, std::endian::little>(uint32_t value)
{
    return le32toh(value);
}
template <>
uint32_t convert<uint32_t, std::endian::big>(uint32_t value)
{
    return be32toh(value);
}
template <>
int32_t convert<int32_t, std::endian::little>(int32_t value)
{
    return le32toh(value);
}
template <>
int32_t convert<int32_t, std::endian::big>(int32_t value)
{
    return be32toh(value);
}

template <>
uint64_t convert<uint64_t, std::endian::little>(uint64_t value)
{
    return le64toh(value);
}
template <>
uint64_t convert<uint64_t, std::endian::big>(uint64_t value)
{
    return be64toh(value);
}
template <>
int64_t convert<int64_t, std::endian::little>(int64_t value)
{
    return le64toh(value);
}
template <>
int64_t convert<int64_t, std::endian::big>(int64_t value)
{
    return be64toh(value);
}

template <>
uint16_t convert_from<uint16_t, std::endian::little>(uint16_t value)
{
    return htole16(value);
}
template <>
uint16_t convert_from<uint16_t, std::endian::big>(uint16_t value)
{
    return htobe16(value);
}
template <>
int16_t convert_from<int16_t, std::endian::little>(int16_t value)
{
    return htole16(value);
}
template <>
int16_t convert_from<int16_t, std::endian::big>(int16_t value)
{
    return htobe16(value);
}

template <>
uint32_t convert_from<uint32_t, std::endian::little>(uint32_t value)
{
    return htole32(value);
}
template <>
uint32_t convert_from<uint32_t, std::endian::big>(uint32_t value)
{
    return htobe32(value);
}
template <>
int32_t convert_from<int32_t, std::endian::little>(int32_t value)
{
    return htole32(value);
}
template <>
int32_t convert_from<int32_t, std::endian::big>(int32_t value)
{
    return htobe32(value);
}

template <>
uint64_t convert_from<uint64_t, std::endian::little>(uint64_t value)
{
    return htole64(value);
}
template <>
uint64_t convert_from<uint64_t, std::endian::big>(uint64_t value)
{
    return htobe64(value);
}
template <>
int64_t convert_from<int64_t, std::endian::little>(int64_t value)
{
    return htole64(value);
}
template <>
int64_t convert_from<int64_t, std::endian::big>(int64_t value)
{
    return htobe64(value);
}

} // namespace sihd::util::endian