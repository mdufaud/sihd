#include <algorithm>
#include <stdexcept>

#include <fmt/format.h>

#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/util/Logger.hpp>

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
#if __BYTE_ORDER == __BIG_ENDIAN
        uint8_t blue;
        uint8_t red;
        uint8_t green;
        uint8_t reserved;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
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

    _bit_per_pixel = bit_per_pixel;
    _width = width;
    _height = height;

    if (bit_per_pixel >= 8)
        _data.resize((width * height) * this->byte_per_pixel());
    else
        _data.resize(std::ceil((width * height) / (float)(8 / bit_per_pixel)));
}

void Bitmap::fill(Pixel pixel)
{
    if (this->empty())
        return;
    const size_t byte_size = this->byte_per_pixel();
    for (size_t i = 0; i < _data.size(); i += byte_size)
    {
        memcpy((void *)(this->c_data() + i), &pixel.value, byte_size);
    }
}

bool Bitmap::is_accessible(size_t row, size_t line) const
{
    return row < _width && line < _height;
}

Pixel Bitmap::get(size_t row, size_t line) const
{
    if (!this->is_accessible(row, line))
    {
        throw std::invalid_argument(
            fmt::format("pixels[{}][{}] is out of bounds (pixels[{}][{}])", line, row, _height, _width));
    }
    const size_t idx = this->coordinate_to_pixel(row, line);
    Pixel ret = 0;
    memcpy(&ret.value, &_data[idx], this->byte_per_pixel());
    return ret;
}

void Bitmap::set(uint8_t *data, size_t size)
{
    if (size > _data.size())
    {
        throw std::invalid_argument(
            fmt::format("pixels data ({}) is out of bounds (pixels[{}])", size, _data.size()));
    }
    memcpy(_data.data(), data, size);
}

void Bitmap::set(size_t row, size_t line, Pixel pixel)
{
    if (!this->is_accessible(row, line))
    {
        throw std::invalid_argument(
            fmt::format("pixels[{}][{}] is out of bounds (pixels[{}][{}])", line, row, _height, _width));
    }
    const size_t idx = this->coordinate_to_pixel(row, line);
    memcpy((void *)(_data.data() + idx), &pixel.value, this->byte_per_pixel());
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

Bitmap::Pixels Bitmap::to_bmp_data() const
{
    if (this->empty())
        return {};

    // BMP rows must be padded to 4-byte boundaries
    const size_t bytes_per_row = _width * this->byte_per_pixel();
    const size_t padded_row_size = ((bytes_per_row + 3) / 4) * 4;
    const size_t image_size = padded_row_size * _height;
    const size_t file_size = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + image_size;

    Pixels bmp_data(file_size, 0);

    // Fill file header
    BitmapFileHeader *file_hdr = reinterpret_cast<BitmapFileHeader *>(bmp_data.data());
    ((unsigned char *)&file_hdr->signature)[0] = 'B';
    ((unsigned char *)&file_hdr->signature)[1] = 'M';
    file_hdr->filesize = file_size;
    file_hdr->reserved = 0;
    file_hdr->fileoffset_to_pixelarray = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

    // Fill info header
    BitmapInfoHeader *info_hdr
        = reinterpret_cast<BitmapInfoHeader *>(bmp_data.data() + sizeof(BitmapFileHeader));
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

    // Copy pixel data with padding
    uint8_t *pixels = bmp_data.data() + file_hdr->fileoffset_to_pixelarray;
    for (size_t y = 0; y < _height; ++y)
    {
        memcpy(pixels + y * padded_row_size, this->c_data() + y * bytes_per_row, bytes_per_row);
        // Padding bytes are already zero from initialization
    }

    return bmp_data;
}

bool Bitmap::save_bmp(std::string_view path) const
{
    File file(path, "wb");

    if (!file.is_open())
        return false;

    Pixels bmp_data = this->to_bmp_data();
    if (bmp_data.empty())
        return false;

    return file.write(bmp_data.data(), bmp_data.size()) == static_cast<ssize_t>(bmp_data.size());
}

bool Bitmap::read_bmp(std::string_view path)
{
    File file(path, "rb");

    if (!file.is_open())
        return false;

    // Get file size
    file.seek_end(0);
    const ssize_t file_size = file.tell();
    if (file_size <= 0)
        return false;

    file.seek_begin(0);

    // Read entire file into memory
    Pixels data(file_size);
    if (file.read(data.data(), file_size) != file_size)
        return false;

    // Parse BMP from memory
    return this->read_bmp_data(data);
}

bool Bitmap::read_bmp_data(const Pixels & data)
{
    this->clear();

    if (data.size() < sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader))
        return false;

    const BitmapFileHeader *file_hdr = reinterpret_cast<const BitmapFileHeader *>(data.data());
    const BitmapInfoHeader *info_hdr
        = reinterpret_cast<const BitmapInfoHeader *>(data.data() + sizeof(BitmapFileHeader));

    // Validate signature
    if (((unsigned char *)&file_hdr->signature)[0] != 'B'
        || ((unsigned char *)&file_hdr->signature)[1] != 'M')
        return false;

    // Handle negative height (top-down DIB)
    const bool top_down = static_cast<int32_t>(info_hdr->height) < 0;
    const uint32_t abs_height
        = top_down ? static_cast<uint32_t>(-static_cast<int32_t>(info_hdr->height)) : info_hdr->height;

    this->create(info_hdr->width, abs_height, info_hdr->bits_per_pixel);

    // Check if we have enough data
    if (data.size() < file_hdr->fileoffset_to_pixelarray)
        return false;

    const uint8_t *pixel_data = data.data() + file_hdr->fileoffset_to_pixelarray;
    const size_t available_size = data.size() - file_hdr->fileoffset_to_pixelarray;

    // BMP rows are padded to 4-byte boundaries
    const size_t bytes_per_row = info_hdr->width * this->byte_per_pixel();
    const size_t padded_row_size = ((bytes_per_row + 3) / 4) * 4;
    const size_t padding = padded_row_size - bytes_per_row;

    bool success = true;
    if (padding == 0)
    {
        // No padding - copy directly
        const size_t data_size = bytes_per_row * abs_height;
        if (available_size >= data_size)
        {
            memcpy(_data.data(), pixel_data, data_size);
        }
        else
        {
            success = false;
        }
    }
    else
    {
        // Has padding - copy row by row
        for (uint32_t y = 0; y < abs_height && success; ++y)
        {
            const size_t row_offset = y * padded_row_size;
            if (row_offset + bytes_per_row <= available_size)
            {
                const size_t dst_y = top_down ? (abs_height - 1 - y) : y;
                memcpy(_data.data() + dst_y * bytes_per_row, pixel_data + row_offset, bytes_per_row);
            }
            else
            {
                success = false;
            }
        }
    }

    if (!success)
        this->clear();

    return success;
}

} // namespace sihd::sys
