#ifndef __SIHD_ZLIB_DEFLATER_HPP__
#define __SIHD_ZLIB_DEFLATER_HPP__

#include <memory>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::zlib
{

class Deflater
{
    public:
        enum class Format
        {
            zlib,
            gzip,
        };

        static constexpr int min_level = 0;
        static constexpr int max_level = 9;
        static constexpr int default_level = 6;

        // throws on a level outside min_level..max_level
        Deflater(Format format = Format::zlib, int level = default_level);
        ~Deflater();

        Deflater(const Deflater &) = delete;
        Deflater & operator=(const Deflater &) = delete;

        // decodable at once, but depends on every previous output of this stream
        sihd::util::Result<sihd::util::ArrByte> deflate_stream(sihd::util::ArrByteView in);
        // ends the stream, a complete one when no deflate_stream call preceded; the next call starts a new one
        sihd::util::Result<sihd::util::ArrByte> deflate_message(sihd::util::ArrByteView in);

        size_t output_bound(size_t input_size) const;

    private:
        struct Impl;

        sihd::util::Result<sihd::util::ArrByte> _deflate(sihd::util::ArrByteView in, int flush);

        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::zlib

#endif
