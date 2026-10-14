#ifndef __SIHD_ZIP_ZIP_HPP__
#define __SIHD_ZIP_ZIP_HPP__

#include <vector>

#include <sihd/util/Result.hpp>

namespace sihd::zip
{

std::vector<std::string> list_entries(const std::string & archive_path);

sihd::util::Result<void> zip(const std::string & root_dir_path, const std::string & archive_path);

sihd::util::Result<void> unzip(const std::string & archive_path, const std::string & unzip_file_path);

} // namespace sihd::zip

#endif