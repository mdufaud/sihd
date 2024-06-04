#ifndef __SIHD_UTIL_BITMAP_HPP__
#define __SIHD_UTIL_BITMAP_HPP__

#include <cstdint>
#include <string_view>
#include <vector>

#include <sihd/util/portable_endian.h>

namespace sihd::util
{

union Pixel
{
        uint32_t value;

#if __BYTE_ORDER == __BIG_ENDIAN
        struct
        {
                uint8_t alpha, red, green, blue;
        };
#elif __BYTE_ORDER == __LITTLE_ENDIAN
        struct
        {
                uint8_t blue, green, red, alpha;
        };
#else
# error Cannot use the Pixel structure
#endif

        constexpr Pixel(): value(0) {}
        constexpr Pixel(uint32_t val): value(val) {}

        constexpr operator uint32_t() const { return value; }
};

class Bitmap
{
    public:
        using Pixels = std::vector<uint8_t>;

        Bitmap();
        // only implemented with 32 and 24 bit per pixel
        Bitmap(size_t width, size_t height, uint8_t bit_per_pixel = 32);
        ~Bitmap() = default;

        // only implemented with 32 and 24 bit per pixel
        void create(size_t width, size_t height, uint8_t bit_per_pixel = 32);
        void fill(Pixel pixel);
        void clear();

        void set(size_t row, size_t line, Pixel pixel);
        Pixel get(size_t row, size_t line) const;
        bool is_accessible(size_t row, size_t line) const;

        bool save_bmp(std::string_view path) const;
        bool read_bmp(std::string_view path);

        bool empty() const { return _data.empty(); };
        size_t height() const { return _height; }
        size_t width() const { return _width; }
        uint8_t byte_per_pixel() const { return _bit_per_pixel / 8; }
        const Pixels & data() const { return _data; }
        const uint8_t *c_data() const;

    protected:
        size_t coordinate_to_pixel(size_t row, size_t line) const;

    private:
        Pixels _data;
        size_t _width;
        size_t _height;
        uint8_t _bit_per_pixel;
};

} // namespace sihd::util

#endif
