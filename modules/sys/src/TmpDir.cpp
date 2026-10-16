#include <filesystem>
#include <stdexcept>
#include <utility>

#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>

namespace sihd::sys
{

TmpDir::TmpDir()
{
    auto path = fs::make_tmp_directory(fs::combine(fs::tmp_path(), "sihd_"));
    if (!path)
        throw std::runtime_error(path.error().message);
    _path = std::move(*path);
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
