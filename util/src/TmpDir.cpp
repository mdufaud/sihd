#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>

namespace sihd::util
{

TmpDir::TmpDir()
{
    _path = fs::make_tmp_directory(fs::ensure_separation(fs::tmp_path()));
}

TmpDir::~TmpDir()
{
    if (fs::is_dir(_path))
    {
        if (fs::remove_directories(_path))
            fs::remove_directory(_path);
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

} // namespace sihd::util