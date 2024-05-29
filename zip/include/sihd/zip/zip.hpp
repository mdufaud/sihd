#ifndef __SIHD_ZIP_ZIP_HPP__
#define __SIHD_ZIP_ZIP_HPP__

#include <string_view>

namespace sihd::zip
{

bool zip(std::string_view root_dir_path, std::string_view archive_path);

bool unzip(std::string_view archive_path, std::string_view unzip_file_path);

} // namespace sihd::zip

#endif