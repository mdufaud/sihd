#include "DirectorySwitcher.hpp"

#define BUF_SIZE 1024

namespace test
{

DirectorySwitcher::DirectorySwitcher(std::string_view path)
{
    if (path.empty() == false)
    {
        char buf[BUF_SIZE];
        if (getcwd(buf, BUF_SIZE) != nullptr)
            _old_cwd = buf;
        if (chdir(path.data()) == 0)
            _cwd = path;
    }
}

DirectorySwitcher::~DirectorySwitcher()
{
    if (_old_cwd.empty() == false)
        chdir(_old_cwd.data());
}

} // namespace test