#include <limits>
#include <stdexcept>

#include <fmt/format.h>

#include <sihd/zlib/Deflater.hpp>

#include "zlib_internal.hpp"

namespace sihd::zlib
{

using namespace sihd::util;

namespace
{

// zlib writes a gzip wrapper when 16 is added to the window bits
constexpr int gzip_window_bits = max_window_bits + 16;
constexpr int default_mem_level = 8;
// the empty stored block a sync flush appends
constexpr size_t sync_flush_marker_size = 5;

} // namespace

struct Deflater::Impl
{
        z_stream stream {};
};

Deflater::Deflater(Format format, int level): _impl(std::make_unique<Impl>())
{
    if (level < min_level || level > max_level)
        throw std::invalid_argument(
            fmt::format("zlib: compression level {} is outside {}..{}", level, min_level, max_level));
    const int window_bits = format == Format::gzip ? gzip_window_bits : max_window_bits;
    const int ret = ::deflateInit2(&_impl->stream,
                                   level,
                                   Z_DEFLATED,
                                   window_bits,
                                   default_mem_level,
                                   Z_DEFAULT_STRATEGY);
    if (ret != Z_OK)
        throw std::runtime_error(zlib_error("deflateInit2", ret, _impl->stream.msg).message);
}

Deflater::~Deflater()
{
    (void)::deflateEnd(&_impl->stream);
}

size_t Deflater::output_bound(size_t input_size) const
{
    // uLong is 32 bits on windows: the bound adds up pieces that fit it on every platform
    constexpr size_t max_piece = std::numeric_limits<uLong>::max();
    size_t bound = sync_flush_marker_size;
    for (; input_size > max_piece; input_size -= max_piece)
        bound += ::deflateBound(&_impl->stream, max_piece);
    return bound + ::deflateBound(&_impl->stream, static_cast<uLong>(input_size));
}

Result<ArrByte> Deflater::deflate_stream(ArrByteView in)
{
    return this->_deflate(in, Z_SYNC_FLUSH);
}

Result<ArrByte> Deflater::deflate_message(ArrByteView in)
{
    Result<ArrByte> out = this->_deflate(in, Z_FINISH);
    (void)::deflateReset(&_impl->stream);
    return out;
}

Result<ArrByte> Deflater::_deflate(ArrByteView in, int flush)
{
    z_stream & stream = _impl->stream;
    ArrByte out;
    if (out.resize(this->output_bound(in.size())) == false)
        return std::unexpected(Error(ErrorCode::out_of_memory, "zlib deflate: cannot allocate its output"));

    // a failed call may leave a pointer into a released input
    stream.avail_in = 0;
    size_t consumed = 0;
    size_t produced = 0;
    while (true)
    {
        feed_input(stream, in, consumed);
        if (produced == out.size() && out.resize(out.size() * 2) == false)
            return std::unexpected(Error(ErrorCode::out_of_memory, "zlib deflate: cannot grow its output"));
        point_output(stream, out, produced);

        const uInt room = stream.avail_out;
        const bool whole_input_given = consumed == in.size();
        const int ret = ::deflate(&stream, whole_input_given ? flush : Z_NO_FLUSH);
        produced += room - stream.avail_out;
        if (ret == Z_STREAM_ERROR)
            return std::unexpected(zlib_error("deflate", ret, stream.msg));

        if (whole_input_given == false || stream.avail_in != 0)
            continue;
        const bool flushed = flush == Z_FINISH ? ret == Z_STREAM_END : stream.avail_out != 0;
        if (flushed)
            break;
    }
    if (out.resize(produced) == false)
        return std::unexpected(Error(ErrorCode::out_of_memory, "zlib: cannot shrink its output"));
    return out;
}

} // namespace sihd::zlib
