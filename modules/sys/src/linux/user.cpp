#include <charconv>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <sihd/sys/cap.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys::user
{

SIHD_NEW_LOGGER("sihd::sys::user");

// === BasicId ===

template <typename Tag>
BasicId<Tag> BasicId<Tag>::from_native(uint32_t id)
{
    BasicId ret;
    ret._id = id;
    ret._valid = true;
    return ret;
}

template <typename Tag>
bool BasicId<Tag>::valid() const
{
    return _valid;
}

template <typename Tag>
std::string BasicId<Tag>::to_string() const
{
    return _valid ? std::to_string(_id) : std::string {};
}

template <typename Tag>
std::optional<BasicId<Tag>> BasicId<Tag>::from_string(std::string_view str)
{
    uint32_t id = 0;
    const char *end = str.data() + str.size();
    const auto [ptr, err] = std::from_chars(str.data(), end, id);
    if (err != std::errc {} || ptr != end)
        return std::nullopt;
    return BasicId::from_native(id);
}

template <typename Tag>
bool BasicId<Tag>::operator==(const BasicId & other) const
{
    return _valid == other._valid && (!_valid || _id == other._id);
}

template class BasicId<UserTag>;
template class BasicId<GroupTag>;

// === identity ===

namespace
{

constexpr size_t buffer_start_size = 1024;
constexpr size_t buffer_max_size = 64 * 1024;

// getpw*_r / getgr*_r all share the same "retry with a bigger buffer on ERANGE" protocol
template <typename Fun>
bool retry_growing_buffer(Fun && fun)
{
    std::vector<char> buffer(buffer_start_size);
    while (true)
    {
        const int ret = fun(buffer.data(), buffer.size());
        if (ret != ERANGE)
            return ret == 0;
        if (buffer.size() >= buffer_max_size)
            return false;
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace

UserId effective_user()
{
    return UserId::from_native(geteuid());
}

UserId real_user()
{
    return UserId::from_native(getuid());
}

GroupId effective_group()
{
    return GroupId::from_native(getegid());
}

GroupId real_group()
{
    return GroupId::from_native(getgid());
}

std::vector<GroupId> groups()
{
    const int total = getgroups(0, nullptr);
    if (total <= 0)
        return {};
    std::vector<gid_t> native(total);
    if (getgroups(total, native.data()) < 0)
        return {};
    std::vector<GroupId> ret;
    ret.reserve(native.size());
    for (gid_t group : native)
        ret.push_back(GroupId::from_native(group));
    return ret;
}

std::optional<std::string> name_of(const UserId & id)
{
    if (!id.valid())
        return std::nullopt;
    std::optional<std::string> ret;
    retry_growing_buffer([&](char *buffer, size_t size) {
        struct passwd pwd;
        struct passwd *result = nullptr;
        const int err = getpwuid_r(id.native(), &pwd, buffer, size, &result);
        if (err == 0 && result != nullptr)
            ret = std::string(result->pw_name);
        return err;
    });
    return ret;
}

std::optional<std::string> name_of(const GroupId & id)
{
    if (!id.valid())
        return std::nullopt;
    std::optional<std::string> ret;
    retry_growing_buffer([&](char *buffer, size_t size) {
        struct group grp;
        struct group *result = nullptr;
        const int err = getgrgid_r(id.native(), &grp, buffer, size, &result);
        if (err == 0 && result != nullptr)
            ret = std::string(result->gr_name);
        return err;
    });
    return ret;
}

std::optional<UserId> user_id_of(std::string_view user_name)
{
    const std::string name_str(user_name);
    std::optional<UserId> ret;
    retry_growing_buffer([&](char *buffer, size_t size) {
        struct passwd pwd;
        struct passwd *result = nullptr;
        const int err = getpwnam_r(name_str.c_str(), &pwd, buffer, size, &result);
        if (err == 0 && result != nullptr)
            ret = UserId::from_native(result->pw_uid);
        return err;
    });
    return ret;
}

std::optional<GroupId> group_id_of(std::string_view group_name)
{
    const std::string name_str(group_name);
    std::optional<GroupId> ret;
    retry_growing_buffer([&](char *buffer, size_t size) {
        struct group grp;
        struct group *result = nullptr;
        const int err = getgrnam_r(name_str.c_str(), &grp, buffer, size, &result);
        if (err == 0 && result != nullptr)
            ret = GroupId::from_native(result->gr_gid);
        return err;
    });
    return ret;
}

std::optional<GroupId> primary_group_of(std::string_view user_name)
{
    const std::string name_str(user_name);
    std::optional<GroupId> ret;
    retry_growing_buffer([&](char *buffer, size_t size) {
        struct passwd pwd;
        struct passwd *result = nullptr;
        const int err = getpwnam_r(name_str.c_str(), &pwd, buffer, size, &result);
        if (err == 0 && result != nullptr)
            ret = GroupId::from_native(result->pw_gid);
        return err;
    });
    return ret;
}

std::optional<GroupId> primary_group_of(const UserId & id)
{
    if (!id.valid())
        return std::nullopt;
    std::optional<GroupId> ret;
    retry_growing_buffer([&](char *buffer, size_t size) {
        struct passwd pwd;
        struct passwd *result = nullptr;
        const int err = getpwuid_r(id.native(), &pwd, buffer, size, &result);
        if (err == 0 && result != nullptr)
            ret = GroupId::from_native(result->pw_gid);
        return err;
    });
    return ret;
}

bool is_root()
{
    return geteuid() == 0;
}

bool drop_privileges(const UserId & user_id, const GroupId & group_id)
{
    if constexpr (!can_drop_privileges)
    {
        return false;
    }
    else
    {
        if (!user_id.valid() || !group_id.valid())
            return false;

        const uid_t uid = user_id.native();
        const gid_t gid = group_id.native();

        // supplementary groups must go first: they can only be cleared while still privileged
        if (setgroups(1, &gid) != 0)
        {
            SIHD_LOG(error, "user: setgroups failed: {}", os::last_error_str());
            return false;
        }
        if (setgid(gid) != 0)
        {
            SIHD_LOG(error, "user: setgid({}) failed: {}", gid, os::last_error_str());
            return false;
        }
        if (setuid(uid) != 0)
        {
            SIHD_LOG(error, "user: setuid({}) failed: {}", uid, os::last_error_str());
            return false;
        }
        // the drop must not be reversible: the process must no longer hold the capabilities that
        // would let it setuid()/setgid() back. Heuristic, not absolute proof, so the id checks
        // below remain authoritative; cap::has() is a no-op (false) where the API is unavailable.
        if (uid != 0 && cap::supported && (cap::has(Cap::setuid) || cap::has(Cap::setgid)))
        {
            SIHD_LOG(error, "user: privileges could be restored after dropping to uid {}", uid);
            return false;
        }
        return getuid() == uid && geteuid() == uid && getgid() == gid && getegid() == gid;
    }
}

} // namespace sihd::sys::user
