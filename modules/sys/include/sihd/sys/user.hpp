#ifndef __SIHD_SYS_USER_HPP__
#define __SIHD_SYS_USER_HPP__

#include <array>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys::user
{

// Opaque account identifier.
//
// linux: a uid_t / gid_t.
// windows: a SID, which is variable sized and cannot be reduced to an integer (the trailing RID
// is not unique across domains) - hence the opaque type instead of a plain uid_t.
//
// Tag only exists to keep user and group identifiers distinct types.
template <typename Tag>
class BasicId
{
    public:
        BasicId() = default;

        [[nodiscard]] bool valid() const;
        // "1000" on linux, "S-1-5-21-..." on windows
        [[nodiscard]] std::string to_string() const;
        [[nodiscard]] static std::optional<BasicId> from_string(std::string_view str);

        [[nodiscard]] bool operator==(const BasicId & other) const;

#if defined(__SIHD_WINDOWS__)
        // SECURITY_MAX_SID_SIZE, checked against the win32 value at build time
        static constexpr size_t max_native_size = 68;

        [[nodiscard]] static BasicId from_native(const void *sid, size_t size);
        [[nodiscard]] const unsigned char *native() const { return _sid.data(); }
        [[nodiscard]] size_t native_size() const { return _size; }

    private:
        std::array<unsigned char, max_native_size> _sid {};
        size_t _size = 0;
#else
        [[nodiscard]] static BasicId from_native(uint32_t id);
        [[nodiscard]] uint32_t native() const { return _id; }

    private:
        uint32_t _id = 0;
        bool _valid = false;
#endif
};

template <typename Tag>
std::ostream & operator<<(std::ostream & stream, const BasicId<Tag> & id)
{
    return stream << id.to_string();
}

struct UserTag;
struct GroupTag;

using UserId = BasicId<UserTag>;
using GroupId = BasicId<GroupTag>;

constexpr bool supported = !sihd::util::build::is_emscripten;
// windows has no setuid: dropping is unix only, see Impersonation for the windows counterpart
constexpr bool can_drop_privileges = sihd::util::build::is_unix && !sihd::util::build::is_emscripten;

// === identity ===
//
// effective_*: identity the CALLING THREAD acts as, the one access checks use. It follows an
// Impersonation. linux: geteuid/getegid. windows: the thread token, falling back to the process
// token when the thread is not impersonating.
//
// real_*: identity the process was started with. It ignores an Impersonation. linux:
// getuid/getgid. windows: always the process token, which has no real/effective distinction.

UserId effective_user();
UserId real_user();
GroupId effective_group();
GroupId real_group();

// name of the effective user
std::optional<std::string> name();
// groups the process belongs to. linux: supplementary groups only. windows: every group SID
// of the effective token, deny-only SIDs excluded
std::vector<GroupId> groups();
// linux: effective user is uid 0 - windows: the token is elevated
bool is_root();

// === lookups ===

std::optional<std::string> name_of(const UserId & id);
std::optional<std::string> name_of(const GroupId & id);
std::optional<UserId> user_id_of(std::string_view user_name);
std::optional<GroupId> group_id_of(std::string_view group_name);
// primary group of a user account, not a group name lookup
std::optional<GroupId> primary_group_of(std::string_view user_name);
std::optional<GroupId> primary_group_of(const UserId & id);

// === privileges ===

// permanently drops to user/group - order matters: setgroups, setgid then setuid
// verifies the drop cannot be reverted before returning true. A failure may leave the process
// partially switched (setgroups runs first and cannot be undone): exit on false if that matters
bool drop_privileges(const UserId & user_id, const GroupId & group_id);
// same, resolving the primary group of the user
bool drop_privileges(std::string_view user_name);

} // namespace sihd::sys::user

#endif
