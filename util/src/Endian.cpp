#include <sihd/util/Endian.hpp>

namespace sihd::util
{

std::string Endian::type_str(Endianness type)
{
    switch (type)
    {
        case BIG:
            return "big";
        case LITTLE:
            return "little";
        default:
            return "unknown";
    }
}

bool Endian::switch_buffer_endianness(void *buf, uint8_t size, size_t buf_size)
{
    int16_t *buf2 = nullptr;
    int32_t *buf4 = nullptr;
    int64_t *buf8 = nullptr;
    size_t i = 0;
    switch (size)
    {
        case 2:
        {
            buf2 = static_cast<int16_t *>(buf);
            while (i < buf_size)
            {
                buf2[i] = bswap_16(buf2[i]);
                ++i;
            }
            break;
        }
        case 4:
        {
            buf4 = static_cast<int32_t *>(buf);
            while (i < buf_size)
            {
                buf4[i] = bswap_32(buf4[i]);
                ++i;
            }
            break;
        }
        case 8:
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
uint16_t Endian::swap<uint16_t>(uint16_t ret)
{
    return bswap_16(ret);
};
template <>
int16_t Endian::swap<int16_t>(int16_t ret)
{
    return bswap_16(ret);
};
template <>
uint32_t Endian::swap<uint32_t>(uint32_t ret)
{
    return bswap_32(ret);
};
template <>
int32_t Endian::swap<int32_t>(int32_t ret)
{
    return bswap_32(ret);
};
template <>
uint64_t Endian::swap<uint64_t>(uint64_t ret)
{
    return bswap_64(ret);
};
template <>
int64_t Endian::swap<int64_t>(int64_t ret)
{
    return bswap_64(ret);
};

template <>
float Endian::swap<float>(float ret)
{
    float *ret_p = &ret;
    uint32_t *tmp = reinterpret_cast<uint32_t *>(ret_p);
    uint32_t swapped = bswap_32(*tmp);
    tmp = &swapped;
    return *(reinterpret_cast<float *>(tmp));
}

template <>
double Endian::swap<double>(double ret)
{
    double *ret_p = &ret;
    uint64_t *tmp = reinterpret_cast<uint64_t *>(ret_p);
    uint64_t swapped = bswap_64(*tmp);
    tmp = &swapped;
    return *(reinterpret_cast<double *>(tmp));
}

template <>
uint16_t Endian::convert<uint16_t, Endian::LITTLE>(uint16_t value)
{
    return le16toh(value);
}
template <>
uint16_t Endian::convert<uint16_t, Endian::BIG>(uint16_t value)
{
    return be16toh(value);
}
template <>
int16_t Endian::convert<int16_t, Endian::LITTLE>(int16_t value)
{
    return le16toh(value);
}
template <>
int16_t Endian::convert<int16_t, Endian::BIG>(int16_t value)
{
    return be16toh(value);
}

template <>
uint32_t Endian::convert<uint32_t, Endian::LITTLE>(uint32_t value)
{
    return le32toh(value);
}
template <>
uint32_t Endian::convert<uint32_t, Endian::BIG>(uint32_t value)
{
    return be32toh(value);
}
template <>
int32_t Endian::convert<int32_t, Endian::LITTLE>(int32_t value)
{
    return le32toh(value);
}
template <>
int32_t Endian::convert<int32_t, Endian::BIG>(int32_t value)
{
    return be32toh(value);
}

template <>
uint64_t Endian::convert<uint64_t, Endian::LITTLE>(uint64_t value)
{
    return le64toh(value);
}
template <>
uint64_t Endian::convert<uint64_t, Endian::BIG>(uint64_t value)
{
    return be64toh(value);
}
template <>
int64_t Endian::convert<int64_t, Endian::LITTLE>(int64_t value)
{
    return le64toh(value);
}
template <>
int64_t Endian::convert<int64_t, Endian::BIG>(int64_t value)
{
    return be64toh(value);
}

template <>
uint16_t Endian::convert_from<uint16_t, Endian::LITTLE>(uint16_t value)
{
    return htole16(value);
}
template <>
uint16_t Endian::convert_from<uint16_t, Endian::BIG>(uint16_t value)
{
    return htobe16(value);
}
template <>
int16_t Endian::convert_from<int16_t, Endian::LITTLE>(int16_t value)
{
    return htole16(value);
}
template <>
int16_t Endian::convert_from<int16_t, Endian::BIG>(int16_t value)
{
    return htobe16(value);
}

template <>
uint32_t Endian::convert_from<uint32_t, Endian::LITTLE>(uint32_t value)
{
    return htole32(value);
}
template <>
uint32_t Endian::convert_from<uint32_t, Endian::BIG>(uint32_t value)
{
    return htobe32(value);
}
template <>
int32_t Endian::convert_from<int32_t, Endian::LITTLE>(int32_t value)
{
    return htole32(value);
}
template <>
int32_t Endian::convert_from<int32_t, Endian::BIG>(int32_t value)
{
    return htobe32(value);
}

template <>
uint64_t Endian::convert_from<uint64_t, Endian::LITTLE>(uint64_t value)
{
    return htole64(value);
}
template <>
uint64_t Endian::convert_from<uint64_t, Endian::BIG>(uint64_t value)
{
    return htobe64(value);
}
template <>
int64_t Endian::convert_from<int64_t, Endian::LITTLE>(int64_t value)
{
    return htole64(value);
}
template <>
int64_t Endian::convert_from<int64_t, Endian::BIG>(int64_t value)
{
    return htobe64(value);
}

} // namespace sihd::util