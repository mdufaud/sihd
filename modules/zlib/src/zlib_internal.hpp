#ifndef __SIHD_ZLIB_ZLIB_INTERNAL_HPP__
#define __SIHD_ZLIB_ZLIB_INTERNAL_HPP__

// next_in is a pointer to const: the input views stay const
#define ZLIB_CONST
#include <zlib.h>

#include <string_view>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Error.hpp>

namespace sihd::zlib
{

constexpr int max_window_bits = 15;

// stream_msg is z_stream::msg, nullptr when zlib left none
sihd::util::Error zlib_error(std::string_view operation, int code, const char *stream_msg);

// hands the next input chunk of at most UINT_MAX bytes once the stream used the previous one
void feed_input(z_stream & stream, sihd::util::ArrByteView in, size_t & consumed);
// points the stream at the room left in out after produced bytes, at most UINT_MAX
void point_output(z_stream & stream, sihd::util::ArrByte & out, size_t produced);

} // namespace sihd::zlib

#endif
