#ifndef __SIHD_UTIL_TOOLS_HPP__
#define __SIHD_UTIL_TOOLS_HPP__

namespace sihd::util::tools
{

template <typename... Conditions>
size_t count_truth(Conditions... conditions)
{
    return (conditions + ...);
}

template <typename... Conditions>
bool only_one_true(Conditions... conditions)
{
    return count_truth<Conditions...>(std::forward<Conditions>(conditions)...) == 1;
}

template <typename... Conditions>
bool maximum_one_true(Conditions... conditions)
{
    return count_truth<Conditions...>(std::forward<Conditions>(conditions)...) <= 1;
}

template <typename... Conditions>
bool only_one_false(Conditions... conditions)
{
    return count_truth<Conditions...>(std::forward<Conditions>(conditions)...) == (sizeof...(Conditions) - 1);
}

template <typename... Conditions>
bool maximum_one_false(Conditions... conditions)
{
    return count_truth<Conditions...>(std::forward<Conditions>(conditions)...) >= (sizeof...(Conditions) - 1);
}

} // namespace sihd::util::tools

#endif
