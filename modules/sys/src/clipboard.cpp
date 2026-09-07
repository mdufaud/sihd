#include <sihd/sys/clipboard.hpp>

// Platform independent clipboard helpers built on the per-platform get_raw/
// set_raw primitives. Modules extend the api with overloads on their types.

namespace sihd::sys::clipboard
{

std::optional<std::string> to_text(const RawContent & raw)
{
    if (raw.mime != sihd::util::mime::utf8_text && raw.mime != sihd::util::mime::plain_text)
        return std::nullopt;
    return std::string(raw.data.cpp_str_view());
}

std::optional<Bitmap> to_image(const RawContent & raw)
{
    if (raw.mime != sihd::util::mime::bmp_image)
        return std::nullopt;
    Bitmap bm;
    if (!bm.read_bmp_data(raw.data))
        return std::nullopt;
    return bm;
}

bool set(std::string_view text)
{
    return set_raw({{sihd::util::mime::utf8_text, text}, {sihd::util::mime::plain_text, text}});
}

bool set(const Bitmap & bitmap)
{
    std::vector<uint8_t> bmp_data = bitmap.to_bmp_data();
    if (bmp_data.empty())
        return false;
    return set_raw({{sihd::util::mime::bmp_image, bmp_data}});
}

bool set(std::string_view mime, sihd::util::ArrByteView data)
{
    return set_raw({{mime, data}});
}

} // namespace sihd::sys::clipboard
