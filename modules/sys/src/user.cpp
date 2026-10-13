#include <sihd/sys/user.hpp>

namespace sihd::sys::user
{

std::optional<std::string> name()
{
    return name_of(effective_user());
}

std::expected<void, sihd::util::Error> drop_privileges(std::string_view user_name)
{
    const auto user_id = user_id_of(user_name);
    const auto group_id = primary_group_of(user_name);
    if (!user_id || !group_id)
        return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_found,
                                                 "cannot drop privileges: no such user '{}'",
                                                 user_name));
    return drop_privileges(*user_id, *group_id);
}

} // namespace sihd::sys::user
