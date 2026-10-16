#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <fmt/core.h>

#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/endian.hpp>

#define BI_RGB 0
#define BI_RLE4 1
#define BI_RLE8 2
#define BI_BITFIELDS 3
#define BI_JPEG 4
#define BI_PNG 5

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

struct ColorTable
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint8_t blue;
        uint8_t red;
        uint8_t green;
        uint8_t reserved;
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        uint8_t reserved;
        uint8_t green;
        uint8_t red;
        uint8_t blue;
#else
# error Cannot use the ColorTable structure
#endif
};

#pragma pack(push, 1)
struct BitmapFileHeader
{
        uint16_t signature;
        uint32_t filesize;
        uint32_t reserved;
        uint32_t fileoffset_to_pixelarray;
};

struct BitmapInfoHeader
{
        uint32_t header_size;
        uint32_t width;
        uint32_t height;
        uint16_t planes;
        uint16_t bits_per_pixel;
        uint32_t compression;
        uint32_t image_size;
        uint32_t pixel_per_meter_x;
        uint32_t pixel_per_meter_y;
        // Specifies the number of color indexes in the color table that are actually used by the bitmap
        uint32_t nb_colors;
        // Specifies the number of color indexes required for displaying the bitmap. If this value is zero,
        // all colors are required.
        uint32_t nb_important_colors;
};
#pragma pack(pop)

void convert_headers(BitmapFileHeader & file, BitmapInfoHeader & info)
{
    file.filesize = endian::convert(file.filesize, std::endian::little);
    file.reserved = endian::convert(file.reserved, std::endian::little);
    file.fileoffset_to_pixelarray = endian::convert(file.fileoffset_to_pixelarray, std::endian::little);
    info.header_size = endian::convert(info.header_size, std::endian::little);
    info.width = endian::convert(info.width, std::endian::little);
    info.height = endian::convert(info.height, std::endian::little);
    info.planes = endian::convert(info.planes, std::endian::little);
    info.bits_per_pixel = endian::convert(info.bits_per_pixel, std::endian::little);
    info.compression = endian::convert(info.compression, std::endian::little);
    info.image_size = endian::convert(info.image_size, std::endian::little);
    info.pixel_per_meter_x = endian::convert(info.pixel_per_meter_x, std::endian::little);
    info.pixel_per_meter_y = endian::convert(info.pixel_per_meter_y, std::endian::little);
    info.nb_colors = endian::convert(info.nb_colors, std::endian::little);
    info.nb_important_colors = endian::convert(info.nb_important_colors, std::endian::little);
}

void store_pixel(uint8_t *data, uint8_t size, Color pixel)
{
    data[0] = pixel.blue;
    data[1] = pixel.green;
    data[2] = pixel.red;
    if (size == 4)
        data[3] = pixel.alpha;
}

uint8_t channel_from_mask(uint32_t value, uint32_t mask)
{
    if (mask == 0)
        return 0;
    unsigned shift = 0;
    while (shift < 32 && ((mask >> shift) & 1) == 0)
        ++shift;
    const uint32_t max = mask >> shift;
    if (max == 0)
        return 0;
    const uint32_t raw = (value & mask) >> shift;
    return static_cast<uint8_t>((static_cast<uint64_t>(raw) * 255) / max);
}

} // namespace

Bitmap::Bitmap(): _width(0), _height(0), _bit_per_pixel(0) {}

Bitmap::Bitmap(size_t width, size_t height, uint8_t bit_per_pixel): Bitmap()
{
    this->create(width, height, bit_per_pixel);
}

void Bitmap::create(size_t width, size_t height, uint8_t bit_per_pixel)
{
    if (bit_per_pixel > 32)
        throw std::invalid_argument("Bitmaps with more than 32 pixels are not supported");

    // implementation limitations
    if (bit_per_pixel < 8)
        throw std::invalid_argument("Bitmaps with less than 8 bit per pixel are not supported");
    if (bit_per_pixel != 24 && bit_per_pixel != 32)
        throw std::invalid_argument("Bitmaps not of 24 or 32 bit per pixels are not yet supported");

    const size_t pixel_size = bit_per_pixel / 8;
    if (height != 0 && width > _data.max_size() / pixel_size / height)
        throw std::length_error("bitmap dimensions exceed the buffer limit");
    _data.resize(width * height * pixel_size);
    _bit_per_pixel = bit_per_pixel;
    _width = width;
    _height = height;
}

void Bitmap::fill(Color pixel)
{
    if (this->empty())
        return;
    const size_t byte_size = this->byte_per_pixel();
    for (size_t i = 0; i < _data.size(); i += byte_size)
    {
        store_pixel(_data.data() + i, byte_size, pixel);
    }
}

bool Bitmap::is_accessible(size_t row, size_t line) const
{
    return row < _width && line < _height;
}

Color Bitmap::get(size_t row, size_t line) const
{
    if (!this->is_accessible(row, line))
    {
        throw std::invalid_argument(
            fmt::format("pixels[{}][{}] is out of bounds (pixels[{}][{}])", line, row, _height, _width));
    }
    const size_t idx = this->coordinate_to_pixel(row, line);
    Color ret = 0;
    ret.blue = _data[idx];
    ret.green = _data[idx + 1];
    ret.red = _data[idx + 2];
    if (this->byte_per_pixel() == 4)
        ret.alpha = _data[idx + 3];
    return ret;
}

void Bitmap::set(uint8_t *data, size_t size)
{
    if (size > _data.size())
    {
        throw std::invalid_argument(fmt::format("pixels data ({}) is out of bounds (pixels[{}])", size, _data.size()));
    }
    memcpy(_data.data(), data, size);
}

void Bitmap::set(size_t row, size_t line, Color pixel)
{
    if (!this->is_accessible(row, line))
    {
        throw std::invalid_argument(
            fmt::format("pixels[{}][{}] is out of bounds (pixels[{}][{}])", line, row, _height, _width));
    }
    const size_t idx = this->coordinate_to_pixel(row, line);
    store_pixel(_data.data() + idx, this->byte_per_pixel(), pixel);
}

void Bitmap::clear()
{
    _width = 0;
    _height = 0;
    _data.clear();
}

const uint8_t *Bitmap::c_data() const
{
    return reinterpret_cast<const uint8_t *>(_data.data());
}

size_t Bitmap::coordinate_to_pixel(size_t row, size_t line) const
{
    return (line * _width + row) * this->byte_per_pixel();
}

Bitmap::PixelBuffer Bitmap::to_bmp_data() const
{
    if (this->empty())
        return {};

    constexpr size_t header_size = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
    constexpr size_t max_image_size = std::numeric_limits<uint32_t>::max() - header_size;
    if (_width > std::numeric_limits<int32_t>::max() || _height > std::numeric_limits<int32_t>::max()
        || _width > max_image_size / this->byte_per_pixel())
        throw std::length_error("bitmap dimensions exceed the BMP limit");
    // BMP rows must be padded to 4-byte boundaries
    const size_t bytes_per_row = _width * this->byte_per_pixel();
    const size_t padded_row_size = ((bytes_per_row + 3) / 4) * 4;
    if (_height > max_image_size / padded_row_size)
        throw std::length_error("bitmap size exceeds the BMP limit");
    const size_t image_size = padded_row_size * _height;
    const size_t file_size = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + image_size;

    PixelBuffer bmp_data(file_size, 0);

    // Fill file header
    BitmapFileHeader *file_hdr = reinterpret_cast<BitmapFileHeader *>(bmp_data.data());
    ((unsigned char *)&file_hdr->signature)[0] = 'B';
    ((unsigned char *)&file_hdr->signature)[1] = 'M';
    file_hdr->filesize = file_size;
    file_hdr->reserved = 0;
    file_hdr->fileoffset_to_pixelarray = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

    // Fill info header
    BitmapInfoHeader *info_hdr = reinterpret_cast<BitmapInfoHeader *>(bmp_data.data() + sizeof(BitmapFileHeader));
    info_hdr->header_size = sizeof(BitmapInfoHeader);
    info_hdr->width = _width;
    info_hdr->height = _height;
    info_hdr->planes = 1;
    info_hdr->bits_per_pixel = _bit_per_pixel;
    info_hdr->compression = BI_RGB;
    info_hdr->image_size = image_size;
    info_hdr->pixel_per_meter_x = 0x130B; // 2835 - 72 dpi
    info_hdr->pixel_per_meter_y = 0x130B; // 2835 - 72 dpi
    info_hdr->nb_colors = 0;
    info_hdr->nb_important_colors = 0;

    convert_headers(*file_hdr, *info_hdr);
    uint8_t *pixels = bmp_data.data() + header_size;
    for (size_t y = 0; y < _height; ++y)
    {
        const size_t src_row = _height - 1 - y;
        memcpy(pixels + y * padded_row_size, this->c_data() + src_row * bytes_per_row, bytes_per_row);
        // Padding bytes are already zero from initialization
    }

    return bmp_data;
}

bool Bitmap::save_bmp(const std::string & path) const
{
    File file(path, "wb");

    if (!file.is_open())
        return false;

    PixelBuffer bmp_data = this->to_bmp_data();
    if (bmp_data.empty())
        return false;

    const auto wrote = file.write(bmp_data.data(), bmp_data.size());
    return wrote && *wrote == bmp_data.size();
}

bool Bitmap::read_bmp(const std::string & path)
{
    File file(path, "rb");

    if (!file.is_open())
        return false;

    // Get file size
    (void)file.seek_end(0);
    const auto file_size = file.tell();
    if (!file_size || *file_size <= 0)
        return false;

    (void)file.seek_begin(0);

    // Read entire file into memory
    PixelBuffer data(*file_size);
    const auto read = file.read(data.data(), *file_size);
    if (!read || *read != (size_t)*file_size)
        return false;

    // Parse BMP from memory
    return this->read_bmp_data(data);
}

bool Bitmap::read_bmp_data(sihd::util::ArrByteView data)
{
    this->clear();

    if (data.size() < sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader))
        return false;

    const auto *bytes = reinterpret_cast<const uint8_t *>(data.data());
    BitmapFileHeader file_header;
    BitmapInfoHeader info_header;
    memcpy(&file_header, bytes, sizeof(file_header));
    memcpy(&info_header, bytes + sizeof(file_header), sizeof(info_header));
    convert_headers(file_header, info_header);
    const auto *file_hdr = &file_header;
    const auto *info_hdr = &info_header;

    // Validate signature
    if (((unsigned char *)&file_hdr->signature)[0] != 'B' || ((unsigned char *)&file_hdr->signature)[1] != 'M')
        return false;

    const uint32_t bpp = info_hdr->bits_per_pixel;
    const uint32_t compression = info_hdr->compression;
    const uint32_t width = info_hdr->width;

    // 12-byte OS/2 header lays fields out differently.
    if (info_hdr->header_size < sizeof(BitmapInfoHeader))
        return false;

    // RLE and jpeg/png payloads need a dedicated decoder.
    if ((compression != BI_RGB && compression != BI_BITFIELDS)
        || (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32)
        || (compression == BI_BITFIELDS && bpp != 16 && bpp != 32))
        return false;

    // Handle negative height (top-down DIB). Unsigned negation: -INT_MIN is
    // undefined on the signed int32 the header stores.
    const bool top_down = static_cast<int32_t>(info_hdr->height) < 0;
    const uint32_t abs_height = top_down ? 0u - info_hdr->height : info_hdr->height;
    if (static_cast<int32_t>(width) <= 0 || abs_height == 0)
        return false;

    const size_t info_start = sizeof(BitmapFileHeader);
    if (info_hdr->header_size > data.size() - info_start)
        return false;
    const size_t header_end = info_start + info_hdr->header_size;
    const size_t pixel_offset = file_hdr->fileoffset_to_pixelarray;
    const bool indexed = bpp <= 8;
    const bool bitfields = compression == BI_BITFIELDS;

    uint32_t red_mask = 0, green_mask = 0, blue_mask = 0, alpha_mask = 0;
    size_t masks_end = header_end;
    if (bitfields)
    {
        const size_t mask_off = info_start + sizeof(BitmapInfoHeader);
        if (mask_off + 12 > data.size())
            return false;
        red_mask = endian::read_le<uint32_t>(bytes + mask_off, 4);
        green_mask = endian::read_le<uint32_t>(bytes + mask_off + 4, 4);
        blue_mask = endian::read_le<uint32_t>(bytes + mask_off + 8, 4);
        if (mask_off + 16 <= pixel_offset && mask_off + 16 <= data.size())
            alpha_mask = endian::read_le<uint32_t>(bytes + mask_off + 12, 4);
        masks_end = std::max(header_end, mask_off + 12);
    }

    // Palette entries are RGBQUAD, or RGB when the space is tight.
    size_t palette_entry_size = 0;
    if (indexed)
    {
        const size_t palette_start = std::max(header_end, masks_end);
        const size_t palette_count = info_hdr->nb_colors != 0 ? info_hdr->nb_colors : (1u << bpp);
        const size_t palette_span = pixel_offset > palette_start ? (pixel_offset - palette_start) : 0;
        if (palette_count <= palette_span / 4)
            palette_entry_size = 4;
        else if (palette_count <= palette_span / 3)
            palette_entry_size = 3;
        else
            return false;
    }

    if (width > (std::numeric_limits<size_t>::max() - 7) / bpp)
        return false;
    const size_t bits_per_row = static_cast<size_t>(width) * bpp;
    const size_t bytes_per_row = (bits_per_row + 7) / 8;
    const size_t padded_row_size = ((bytes_per_row + 3) / 4) * 4;

    // Divide instead of multiply: width*rows overflows size_t.
    if (bytes_per_row > data.size() || pixel_offset > data.size() || pixel_offset < masks_end
        || abs_height > (data.size() - pixel_offset) / padded_row_size)
        return false;

    if (abs_height > 0 && width > std::numeric_limits<size_t>::max() / abs_height)
        return false;
    const size_t pixels = static_cast<size_t>(width) * abs_height;
    if (pixels > std::numeric_limits<size_t>::max() / 4)
        return false;

    // Color is 32-bit, so decode every depth to 32bpp.
    this->create(width, abs_height, 32);

    const uint8_t *pixel_data = bytes + pixel_offset;
    const size_t available_size = data.size() - pixel_offset;
    const uint8_t *palette = indexed ? bytes + std::max(header_end, masks_end) : nullptr;
    const size_t palette_count = indexed ? (info_hdr->nb_colors != 0 ? info_hdr->nb_colors : (1u << bpp)) : 0;

    for (uint32_t y = 0; y < abs_height; ++y)
    {
        const size_t row_off = y * padded_row_size;
        if (row_off + bytes_per_row > available_size)
        {
            this->clear();
            return false;
        }
        const uint8_t *row = pixel_data + row_off;
        const uint32_t dst_y = top_down ? y : (abs_height - 1 - y);

        for (uint32_t x = 0; x < width; ++x)
        {
            Color c;
            if (indexed)
            {
                uint32_t idx;
                if (bpp == 8)
                    idx = row[x];
                else if (bpp == 4)
                    idx = (row[x / 2] >> ((x & 1) ? 0 : 4)) & 0xF;
                else // bpp == 1
                    idx = (row[x / 8] >> (7 - (x & 7))) & 0x1;
                if (idx >= palette_count)
                    idx = 0;
                // Stored BGR.
                const uint8_t *entry = palette + static_cast<size_t>(idx) * palette_entry_size;
                c.red = entry[2];
                c.green = entry[1];
                c.blue = entry[0];
                c.alpha = 255;
            }
            else
            {
                uint32_t packed = 0;
                if (bpp == 32)
                    packed = endian::read_le<uint32_t>(row + x * 4, sizeof(uint32_t));
                else if (bpp == 24)
                    packed = endian::read_le<uint32_t>(row + x * 3, 3);
                else // bpp == 16
                    packed = endian::read_le<uint16_t>(row + x * 2, sizeof(uint16_t));

                if (bitfields)
                {
                    c.red = channel_from_mask(packed, red_mask);
                    c.green = channel_from_mask(packed, green_mask);
                    c.blue = channel_from_mask(packed, blue_mask);
                    c.alpha = alpha_mask != 0 ? channel_from_mask(packed, alpha_mask) : 255;
                }
                else if (bpp == 32)
                {
                    // Keep the stored 4th byte as alpha so the format round-trips.
                    c.red = row[x * 4 + 2];
                    c.green = row[x * 4 + 1];
                    c.blue = row[x * 4];
                    c.alpha = row[x * 4 + 3];
                }
                else if (bpp == 24)
                {
                    c.red = row[x * 3 + 2];
                    c.green = row[x * 3 + 1];
                    c.blue = row[x * 3];
                    c.alpha = 255;
                }
                else // BI_RGB 16bpp is 5-5-5
                {
                    c.red = channel_from_mask(packed, 0x7C00);
                    c.green = channel_from_mask(packed, 0x03E0);
                    c.blue = channel_from_mask(packed, 0x001F);
                    c.alpha = 255;
                }
            }
            this->set(x, dst_y, c);
        }
    }

    return true;
}

} // namespace sihd::sys
