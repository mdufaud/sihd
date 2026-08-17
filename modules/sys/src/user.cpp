#include <sihd/sys/user.hpp>

namespace sihd::sys::user
{

std::optional<std::string> name()
{
    return name_of(effective_user());
}

bool drop_privileges(std::string_view user_name)
{
    const auto user_id = user_id_of(user_name);
    if (!user_id.has_value())
        return false;
    const auto group_id = primary_group_of(user_name);
    if (!group_id.has_value())
        return false;
    return drop_privileges(*user_id, *group_id);
}

} // namespace sihd::sys::user
