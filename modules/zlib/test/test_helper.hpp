#ifndef __SIHD_ZLIB_TEST_HELPER_HPP__
#define __SIHD_ZLIB_TEST_HELPER_HPP__

#include <string>
#include <string_view>

#include <sihd/core/Channel.hpp>
#include <sihd/zlib/Deflater.hpp>

namespace test
{

std::string text_of(size_t size);
std::string noise_of(size_t size);
std::string compress_message(const std::string & text,
                             sihd::zlib::Deflater::Format format = sihd::zlib::Deflater::Format::zlib);
std::string_view view_of(const sihd::core::Channel *channel);

} // namespace test

#endif
