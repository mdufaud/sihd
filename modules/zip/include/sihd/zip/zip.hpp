#ifndef __SIHD_ZIP_ZIP_HPP__
#define __SIHD_ZIP_ZIP_HPP__

#include <expected>
#include <string_view>
#include <vector>

#include <sihd/util/Error.hpp>

namespace sihd::zip
{

std::vector<std::string> list_entries(std::string_view archive_path);

std::expected<void, sihd::util::Error> zip(std::string_view root_dir_path, std::string_view archive_path);

std::expected<void, sihd::util::Error> unzip(std::string_view archive_path, std::string_view unzip_file_path);

} // namespace sihd::zip

#endif