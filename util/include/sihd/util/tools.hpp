#ifndef __SIHD_UTIL_TOOLS_HPP__
#define __SIHD_UTIL_TOOLS_HPP__

namespace sihd::util::tools
{

template <typename... Conditions>
bool only_one_true(Conditions... conditions)
{
    size_t i = 0;
    i = (conditions + ...);
    return i == 1;
}

template <typename... Conditions>
bool maximum_one_true(Conditions... conditions)
{
    size_t i = 0;
    i = (conditions + ...);
    return i <= 1;
}

template <typename... Conditions>
bool only_one_false(Conditions... conditions)
{
    size_t i = 0;
    i = (conditions + ...);
    return i == (sizeof...(Conditions) - 1);
}

template <typename... Conditions>
bool maximum_one_false(Conditions... conditions)
{
    size_t i = 0;
    i = (conditions + ...);
    return i >= (sizeof...(Conditions) - 1);
}

} // namespace sihd::util::tools

#endif
