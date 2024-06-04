#include <algorithm>
#include <stdexcept>

#include <fmt/format.h>

#include <sihd/util/Bitmap.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/Logger.hpp>

#define BI_RGB 0
#define BI_RLE4 1
#define BI_RLE8 2

namespace sihd::util
{

SIHD_LOGGER;

namespace
{

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
        uint32_t num_colors_palette;
        uint32_t most_important_color;
};
#pragma pack(pop)

} // namespace

Bitmap::Bitmap(): _width(0), _height(0), _bit_per_pixel(24) {}

Bitmap::Bitmap(size_t width, size_t height): Bitmap()
{
    this->create(width, height);
}

void Bitmap::create(size_t width, size_t height)
{
    _width = width;
    _height = height;
    _data.resize(width * height * this->byte_per_pixel());
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

bool Bitmap::save_bmp(std::string_view path) const
{
    File file(path, "wb");

    if (!file.is_open())
        return false;

    BitmapFileHeader bitmap_file_hdr;
    ((unsigned char *)&bitmap_file_hdr.signature)[0] = 'B';
    ((unsigned char *)&bitmap_file_hdr.signature)[1] = 'M';
    bitmap_file_hdr.filesize = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + _data.size();
    bitmap_file_hdr.reserved = 0;
    bitmap_file_hdr.fileoffset_to_pixelarray = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

    BitmapInfoHeader bitmap_info_hdr;
    bitmap_info_hdr.header_size = sizeof(BitmapInfoHeader);
    bitmap_info_hdr.width = _width;
    bitmap_info_hdr.height = _height;
    bitmap_info_hdr.planes = 1;
    bitmap_info_hdr.bits_per_pixel = _bit_per_pixel;
    bitmap_info_hdr.compression = BI_RGB;
    bitmap_info_hdr.image_size = _data.size();
    bitmap_info_hdr.pixel_per_meter_x = 0x130B; // 2835 - 72 dpi
    bitmap_info_hdr.pixel_per_meter_y = 0x130B; // 2835 - 72 dpi
    bitmap_info_hdr.num_colors_palette = 0;
    bitmap_info_hdr.most_important_color = 0;

    bool success = file.write(&bitmap_file_hdr, sizeof(BitmapFileHeader)) == sizeof(BitmapFileHeader);
    success = success && file.write(&bitmap_info_hdr, sizeof(BitmapInfoHeader)) == sizeof(BitmapInfoHeader);
    success = success && file.write(this->c_data(), _data.size()) == (ssize_t)_data.size();

    return success;
}

bool Bitmap::read_bmp(std::string_view path)
{
    this->clear();

    File file(path, "rb");

    if (!file.is_open())
        return false;

    BitmapFileHeader bitmap_file_hdr;
    BitmapInfoHeader bitmap_info_hdr;

    bool success = file.read(&bitmap_file_hdr, sizeof(BitmapFileHeader)) == sizeof(BitmapFileHeader);
    success = success && file.read(&bitmap_info_hdr, sizeof(BitmapInfoHeader)) == sizeof(BitmapInfoHeader);

    if (success)
    {
        _bit_per_pixel = bitmap_info_hdr.bits_per_pixel;
        this->create(bitmap_info_hdr.width, bitmap_info_hdr.height);
    }

    success = success && file.read(_data.data(), bitmap_info_hdr.image_size) == bitmap_info_hdr.image_size;

    return success;
}

} // namespace sihd::util
