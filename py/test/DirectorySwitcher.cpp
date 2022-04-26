#include "DirectorySwitcher.hpp"

#define BUF_SIZE 1024

namespace test
{

DirectorySwitcher::DirectorySwitcher(std::string_view path)
{
    if (path.empty() == false)
    {
        char buf[BUF_SIZE];
        getcwd(buf, BUF_SIZE);
        _old_cwd = buf;
        chdir(path.data());
    }
}

DirectorySwitcher::~DirectorySwitcher()
{
    if (_old_cwd.empty() == false)
        chdir(_old_cwd.data());
}

}