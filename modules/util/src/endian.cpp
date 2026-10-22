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
    size_t i = 0;
    switch (size)
    {
        case ByteSize::b2:
        {
            auto *buf2 = static_cast<int16_t *>(buf);
            while (i < buf_size)
            {
                buf2[i] = std::byteswap(buf2[i]);
                ++i;
            }
            break;
        }

        case ByteSize::b4:
        {
            auto *buf4 = static_cast<int32_t *>(buf);
            while (i < buf_size)
            {
                buf4[i] = std::byteswap(buf4[i]);
                ++i;
            }
            break;
        }

        case ByteSize::b8:
        {
            auto *buf8 = static_cast<int64_t *>(buf);
            while (i < buf_size)
            {
                buf8[i] = std::byteswap(buf8[i]);
                ++i;
            }
            break;
        }
        default:
            return false;
    }
    return true;
}

} // namespace sihd::util::endian
