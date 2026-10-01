#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/zip/ZipFile.hpp>
#include <sihd/zip/zip.hpp>

using namespace sihd::util;
using namespace sihd::sys;

namespace sihd::zip
{

SIHD_LOGGER;

std::vector<std::string> list_entries(std::string_view archive_path)
{
    constexpr bool read_only = true;
    constexpr bool do_strict_checks = true;
    std::vector<std::string> ret;

    ZipFile zip(archive_path, read_only, do_strict_checks);

    if (!zip.is_open())
        return ret;

    for (;;)
    {
        auto next = zip.read_next_entry();
        if (SIHD_UNEXPECTED_LOG(next))
            break;
        if (!next.value())
            break;
        ret.emplace_back(zip.entry_name());
    }

    return ret;
}

std::expected<void, Error> zip(std::string_view root_dir_path, std::string_view archive_path)
{
    constexpr bool read_only = false;
    constexpr bool do_strict_checks = true;

    ZipFile zip;
    auto opened = zip.open(archive_path, read_only, do_strict_checks);
    SIHD_UNEXPECTED_RETURN(opened);

    auto added = zip.add_from_fs(fs::filename(root_dir_path), root_dir_path);
    SIHD_UNEXPECTED_RETURN(added);

    return zip.close();
}

std::expected<void, Error> unzip(std::string_view archive_path, std::string_view unzip_file_path)
{
    constexpr bool read_only = true;
    constexpr bool do_strict_checks = true;

    ZipFile zip;
    auto opened = zip.open(archive_path, read_only, do_strict_checks);
    SIHD_UNEXPECTED_RETURN(opened);

    auto dir = fs::make_directory(unzip_file_path, 0750);
    SIHD_UNEXPECTED_RETURN(dir);

    for (;;)
    {
        auto next = zip.read_next_entry();
        SIHD_UNEXPECTED_RETURN(next);
        if (!next.value())
            break;
        auto dumped = zip.dump_entry_to_fs(fs::combine(unzip_file_path, zip.entry_name()));
        SIHD_UNEXPECTED_RETURN(dumped);
    }

    return {};
}

} // namespace sihd::zip
