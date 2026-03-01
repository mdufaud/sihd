#include <filesystem>
#include <random>

#include <sihd/sys/TmpDir.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::sys
{

namespace
{

std::string make_tmp_dir()
{
    auto base = std::filesystem::temp_directory_path();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist;
    for (int i = 0; i < 100; ++i)
    {
        auto path = base / ("sihd_" + std::to_string(dist(gen)));
        std::error_code ec;
        if (std::filesystem::create_directory(path, ec) && !ec)
            return path.string();
    }
    return {};
}

} // namespace

TmpDir::TmpDir()
{
    _path = make_tmp_dir();
}

TmpDir::~TmpDir()
{
    if (!_path.empty())
    {
        std::error_code ec;
        std::filesystem::remove_all(_path, ec);
    }
};

TmpDir::operator bool() const
{
    return _path.empty() == false;
}

TmpDir::operator std::string() const
{
    return _path;
}

TmpDir::operator std::string_view() const
{
    return std::string_view(_path);
}

const std::string & TmpDir::path() const
{
    return _path;
}

} // namespace sihd::sys