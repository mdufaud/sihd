#include <sihd/util/Duration.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

std::string Duration::str(bool total_parenthesis, bool nano_resolution) const
{
    return str::timeoffset_str(_nano, total_parenthesis, nano_resolution);
}

} // namespace sihd::util
