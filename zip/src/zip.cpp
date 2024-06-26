#include <fmt/format.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>

#include <sihd/zip/ZipFile.hpp>
#include <sihd/zip/zip.hpp>

namespace sihd::zip
{

SIHD_LOGGER;

using namespace sihd::util;

std::vector<std::string> list_entries(std::string_view archive_path)
{
    constexpr bool read_only = true;
    constexpr bool do_strict_checks = true;
    std::vector<std::string> ret;

    ZipFile zip(archive_path, read_only, do_strict_checks);

    if (!zip.is_open())
        return ret;

    while (zip.read_next_entry())
    {
        ret.emplace_back(zip.entry_name());
    }

    return ret;
}

bool zip(std::string_view root_dir_path, std::string_view archive_path)
{
    constexpr bool read_only = false;
    constexpr bool do_strict_checks = true;

    ZipFile zip(archive_path, read_only, do_strict_checks);

    if (zip.is_open() == false)
        return false;

    const bool success = zip.add_from_fs(fs::filename(root_dir_path), root_dir_path);
    // commit changes
    if (success)
        zip.close();

    return success;
}

bool unzip(std::string_view archive_path, std::string_view unzip_file_path)
{
    constexpr bool read_only = true;
    constexpr bool do_strict_checks = true;

    ZipFile zip(archive_path, read_only, do_strict_checks);

    if (zip.is_open() == false)
        return false;

    if (fs::make_directory(unzip_file_path, 0750) == false)
        return false;

    while (zip.read_next_entry())
    {
        if (zip.dump_entry_to_fs(fs::combine(unzip_file_path, zip.entry_name())) == false)
            return false;
    }

    return true;
}

} // namespace sihd::zip