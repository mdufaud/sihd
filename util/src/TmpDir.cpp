#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>

namespace sihd::util
{

TmpDir::TmpDir(std::string_view path)
{
    if (path.empty() == false)
        _path = fs::make_tmp_directory(path);
}

TmpDir::TmpDir(): TmpDir(fs::ensure_separation(fs::tmp_path()))
{
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

const std::string & TmpDir::path() const
{
    return _path;
}

}