#include "DirectorySwitcher.hpp"

#include <array>

namespace test
{

namespace
{
constexpr size_t buffer_size = 1024;
} // namespace

DirectorySwitcher::DirectorySwitcher(std::string_view path)
{
    if (path.empty() == false)
    {
        std::array<char, buffer_size> buf {};
        if (getcwd(buf.data(), buf.size()) != nullptr)
            _old_cwd = buf.data();
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