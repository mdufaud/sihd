#ifndef __SIHD_UTIL_TMPDIR_HPP__
# define __SIHD_UTIL_TMPDIR_HPP__

# include <sihd/util/FS.hpp>

namespace sihd::util
{

class TmpDir
{
    public:
        TmpDir()
        {
            _path = FS::make_tmp_directory(FS::ensure_separation(FS::tmp_path()));
        }

        TmpDir(std::string_view prefix)
        {
            _path = FS::make_tmp_directory(prefix);
        }

        ~TmpDir()
        {
            if (FS::is_dir(_path))
            {
                if (FS::remove_directories(_path))
                    FS::remove_directory(_path);
            }
        };

        const std::string & path() const { return _path; }

    private:
        std::string _path;
};

}

#endif