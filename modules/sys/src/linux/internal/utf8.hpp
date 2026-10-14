#ifndef __SIHD_SYS_LINUX_INTERNAL_UTF8_HPP__
#define __SIHD_SYS_LINUX_INTERNAL_UTF8_HPP__

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace sihd::sys::internal
{

inline std::optional<std::vector<char32_t>> utf8_decode(std::string_view utf8)
{
    std::vector<char32_t> out;
    out.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size();)
    {
        const auto byte = static_cast<unsigned char>(utf8[i]);
        size_t len = 0;
        char32_t cp = 0;
        if (byte < 0x80)
        {
            len = 1;
            cp = byte;
        }
        else if ((byte & 0xE0) == 0xC0)
        {
            len = 2;
            cp = byte & 0x1F;
        }
        else if ((byte & 0xF0) == 0xE0)
        {
            len = 3;
            cp = byte & 0x0F;
        }
        else if ((byte & 0xF8) == 0xF0)
        {
            len = 4;
            cp = byte & 0x07;
        }
        else
            return std::nullopt;
        if (i + len > utf8.size())
            return std::nullopt;
        for (size_t k = 1; k < len; ++k)
        {
            const auto cont = static_cast<unsigned char>(utf8[i + k]);
            if ((cont & 0xC0) != 0x80)
                return std::nullopt;
            cp = (cp << 6) | (cont & 0x3F);
        }
        if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000) || cp > 0x10FFFF
            || (cp >= 0xD800 && cp <= 0xDFFF))
            return std::nullopt;
        out.push_back(cp);
        i += len;
    }
    return out;
}

} // namespace sihd::sys::internal

#endif
