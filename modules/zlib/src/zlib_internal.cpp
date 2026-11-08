#include "zlib_internal.hpp"

#include <algorithm>
#include <climits>

namespace sihd::zlib
{

using namespace sihd::util;

namespace
{

ErrorCode error_code_from_zlib(int code)
{
    switch (code)
    {
        case Z_DATA_ERROR:
        case Z_NEED_DICT:
            return ErrorCode::invalid_argument;
        case Z_MEM_ERROR:
            return ErrorCode::out_of_memory;
        case Z_VERSION_ERROR:
            return ErrorCode::not_supported;
        default:
            return ErrorCode::unknown;
    }
}

uInt clamp_to_uint(size_t size)
{
    return static_cast<uInt>(std::min<size_t>(size, UINT_MAX));
}

} // namespace

Error zlib_error(std::string_view operation, int code, const char *stream_msg)
{
    if (stream_msg != nullptr)
        return Error(error_code_from_zlib(code), "zlib {}: {} ({})", operation, zError(code), stream_msg);
    return Error(error_code_from_zlib(code), "zlib {}: {}", operation, zError(code));
}

void feed_input(z_stream & stream, ArrByteView in, size_t & consumed)
{
    if (stream.avail_in != 0)
        return;
    stream.next_in = reinterpret_cast<const Bytef *>(in.data() + consumed);
    stream.avail_in = clamp_to_uint(in.size() - consumed);
    consumed += stream.avail_in;
}

void point_output(z_stream & stream, ArrByte & out, size_t produced)
{
    stream.next_out = reinterpret_cast<Bytef *>(out.data() + produced);
    stream.avail_out = clamp_to_uint(out.size() - produced);
}

} // namespace sihd::zlib
