#include <algorithm>
#include <stdexcept>

#include <sihd/zlib/Inflater.hpp>

#include "zlib_internal.hpp"

namespace sihd::zlib
{

using namespace sihd::util;

namespace
{

// zlib detects a zlib or a gzip header when 32 is added to the window bits
constexpr int any_header_window_bits = max_window_bits + 32;
constexpr size_t first_output_size = 16384;

Result<int> inflate_once(z_stream & stream,
                         ArrByteView in,
                         size_t & consumed,
                         ArrByte & out,
                         size_t & produced,
                         size_t max_output_size)
{
    feed_input(stream, in, consumed);
    if (produced == out.size())
    {
        // one byte past the limit tells an overflow from an output of exactly the limit
        const size_t grown_size = std::min(std::max(out.size() * 2, first_output_size), max_output_size + 1);
        if (out.resize(grown_size) == false)
            return std::unexpected(Error(ErrorCode::out_of_memory, "zlib inflate: cannot grow its output"));
    }
    point_output(stream, out, produced);

    const uInt room = stream.avail_out;
    const int ret = ::inflate(&stream, Z_NO_FLUSH);
    produced += room - stream.avail_out;
    if (produced > max_output_size)
        return std::unexpected(Error(ErrorCode::overflow, "zlib inflate: output exceeds {} bytes", max_output_size));
    return ret;
}

} // namespace

struct Inflater::Impl
{
        z_stream stream {};
        size_t max_output_size = 0;
};

Inflater::Inflater(size_t max_output_size): _impl(std::make_unique<Impl>())
{
    _impl->max_output_size = max_output_size;
    const int ret = ::inflateInit2(&_impl->stream, any_header_window_bits);
    if (ret != Z_OK)
        throw std::runtime_error(zlib_error("inflateInit2", ret, _impl->stream.msg).message);
}

Inflater::~Inflater()
{
    (void)::inflateEnd(&_impl->stream);
}

Result<ArrByte> Inflater::inflate_stream(ArrByteView in)
{
    Result<ArrByte> out = this->_inflate(in, false);
    if (!out)
        (void)::inflateReset(&_impl->stream);
    return out;
}

Result<ArrByte> Inflater::inflate_message(ArrByteView in)
{
    Result<ArrByte> out = this->_inflate(in, true);
    (void)::inflateReset(&_impl->stream);
    return out;
}

Result<ArrByte> Inflater::_inflate(ArrByteView in, bool single_message)
{
    z_stream & stream = _impl->stream;
    ArrByte out;

    // a failed call may leave a pointer into a released input
    stream.avail_in = 0;
    size_t consumed = 0;
    size_t produced = 0;
    int ret = Z_OK;
    bool input_left = false;
    while (true)
    {
        Result<int> step = inflate_once(stream, in, consumed, out, produced, _impl->max_output_size);
        SIHD_UNEXPECTED_RETURN(step);
        ret = *step;
        input_left = stream.avail_in != 0 || consumed != in.size();

        if (ret == Z_STREAM_END)
        {
            // a stream that ended restarts on the next header
            (void)::inflateReset(&stream);
            if (single_message || input_left == false)
                break;
            continue;
        }
        // no progress left: every input byte is consumed
        if (ret == Z_BUF_ERROR)
            break;
        if (ret != Z_OK)
            return std::unexpected(zlib_error("inflate", ret, stream.msg));
        if (input_left == false && stream.avail_out != 0)
            break;
    }

    if (single_message && ret != Z_STREAM_END)
        return std::unexpected(Error(ErrorCode::invalid_argument, "zlib inflate: truncated message"));
    if (single_message && input_left)
        return std::unexpected(Error(ErrorCode::invalid_argument, "zlib inflate: bytes after the end of the message"));
    if (out.resize(produced) == false)
        return std::unexpected(Error(ErrorCode::out_of_memory, "zlib: cannot shrink its output"));
    return out;
}

} // namespace sihd::zlib
