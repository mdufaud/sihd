#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <sihd/sys/fs.hpp>

using namespace sihd::sys;

namespace
{

const char *mount_str(fs::MountType type)
{
    switch (type)
    {
        case fs::MountType::local:
            return "local";
        case fs::MountType::network:
            return "network";
        case fs::MountType::ram:
            return "ram";
        case fs::MountType::readonly:
            return "readonly";
        case fs::MountType::unknown:
            return "unknown";
    }
    return "unknown";
}

const char *medium_str(fs::StorageMedium medium)
{
    switch (medium)
    {
        case fs::StorageMedium::ssd:
            return "ssd";
        case fs::StorageMedium::hdd:
            return "hdd";
        case fs::StorageMedium::unknown:
            return "unknown";
    }
    return "unknown";
}

void dump(std::string_view path)
{
    fmt::print("path        : {}\n", path);
    fmt::print("  exists    : {}\n", fs::exists(path));
    fmt::print("  is_dir    : {}\n", fs::is_dir(path));
    fmt::print("  is_file   : {}\n", fs::is_file(path));
    fmt::print("  realpath  : {}\n", fs::realpath(path));
    fmt::print("  mount     : {}\n", mount_str(fs::mount_type(path)));
    fmt::print("  medium    : {}\n", medium_str(fs::storage_medium(path)));
    fmt::print("\n");
}

} // namespace

int main(int argc, char **argv)
{
    CLI::App app {"Dump filesystem/storage info for paths"};

    std::vector<std::string> paths;
    app.add_option("paths", paths, "paths to inspect (default: cwd)");

    CLI11_PARSE(app, argc, argv);

    if (paths.empty())
        paths.push_back(fs::cwd());

    for (const auto & path : paths)
        dump(path);

    return 0;
}
