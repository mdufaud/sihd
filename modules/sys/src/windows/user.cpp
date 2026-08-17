// clang-format off
#include <windows.h>
#include <sddl.h> // ConvertSidToStringSidA
// clang-format on

#include <vector>

#include <sihd/sys/user.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys::user
{

SIHD_NEW_LOGGER("sihd::sys::user");

static_assert(UserId::max_native_size == SECURITY_MAX_SID_SIZE,
              "BasicId inline storage must hold the largest possible SID");

// === BasicId ===

template <typename Tag>
BasicId<Tag> BasicId<Tag>::from_native(const void *sid, size_t size)
{
    BasicId ret;
    if (sid != nullptr && size > 0 && size <= max_native_size)
    {
        memcpy(ret._sid.data(), sid, size);
        ret._size = size;
    }
    return ret;
}

template <typename Tag>
bool BasicId<Tag>::valid() const
{
    return _size > 0;
}

template <typename Tag>
std::string BasicId<Tag>::to_string() const
{
    if (!this->valid())
        return {};
    char *str = nullptr;
    // const_cast: win32 takes a non-const PSID but only reads it
    if (ConvertSidToStringSidA(const_cast<unsigned char *>(_sid.data()), &str) == 0 || str == nullptr)
        return {};
    std::string ret(str);
    LocalFree(str);
    return ret;
}

template <typename Tag>
std::optional<BasicId<Tag>> BasicId<Tag>::from_string(std::string_view str)
{
    const std::string str_copy(str);
    PSID sid = nullptr;
    if (ConvertStringSidToSidA(str_copy.c_str(), &sid) == 0 || sid == nullptr)
        return std::nullopt;
    const DWORD size = GetLengthSid(sid);
    BasicId ret = BasicId::from_native(sid, size);
    LocalFree(sid);
    if (!ret.valid())
        return std::nullopt;
    return ret;
}

template <typename Tag>
bool BasicId<Tag>::operator==(const BasicId & other) const
{
    if (_size == 0 || other._size == 0)
        return _size == other._size;
    return EqualSid(const_cast<unsigned char *>(_sid.data()), const_cast<unsigned char *>(other._sid.data())) != 0;
}

template class BasicId<UserTag>;
template class BasicId<GroupTag>;

// === identity ===

namespace
{

// RAII around an identity token: every identity query reads from one
class IdentityToken
{
    public:
        // effective identity: the thread token when impersonating, the process token otherwise -
        // OpenThreadToken fails with ERROR_NO_TOKEN when the thread carries none
        static IdentityToken effective(DWORD access = TOKEN_QUERY)
        {
            IdentityToken ret;
            if (OpenThreadToken(GetCurrentThread(), access, TRUE, &ret._token) == 0)
                ret._token = nullptr;
            if (ret._token == nullptr)
                return IdentityToken::process(access);
            return ret;
        }

        static IdentityToken process(DWORD access = TOKEN_QUERY)
        {
            IdentityToken ret;
            if (OpenProcessToken(GetCurrentProcess(), access, &ret._token) == 0)
                ret._token = nullptr;
            return ret;
        }

        IdentityToken() = default;

        ~IdentityToken()
        {
            if (_token != nullptr)
                CloseHandle(_token);
        }

        IdentityToken(const IdentityToken &) = delete;
        IdentityToken & operator=(const IdentityToken &) = delete;

        IdentityToken(IdentityToken && other): _token(other._token) { other._token = nullptr; }
        IdentityToken & operator=(IdentityToken &&) = delete;

        bool valid() const { return _token != nullptr; }
        HANDLE handle() const { return _token; }

        // reads a variable sized TOKEN_* structure out of the token
        std::vector<BYTE> information(TOKEN_INFORMATION_CLASS type) const
        {
            if (_token == nullptr)
                return {};
            DWORD size = 0;
            GetTokenInformation(_token, type, nullptr, 0, &size);
            if (size == 0)
                return {};
            std::vector<BYTE> buffer(size);
            if (GetTokenInformation(_token, type, buffer.data(), size, &size) == 0)
                return {};
            return buffer;
        }

    private:
        HANDLE _token = nullptr;
};

template <typename Id>
Id id_from_sid(PSID sid)
{
    if (sid == nullptr || IsValidSid(sid) == 0)
        return {};
    return Id::from_native(sid, GetLengthSid(sid));
}

// windows accounts are looked up by name through the local authority
template <typename Id>
std::optional<Id> lookup_account(std::string_view account_name)
{
    const std::string name_str(account_name);
    std::vector<BYTE> sid(SECURITY_MAX_SID_SIZE);
    DWORD sid_size = static_cast<DWORD>(sid.size());
    DWORD domain_size = 0;
    SID_NAME_USE use;

    // first call sizes the domain buffer
    LookupAccountNameA(nullptr, name_str.c_str(), sid.data(), &sid_size, nullptr, &domain_size, &use);
    if (domain_size == 0)
        return std::nullopt;

    std::vector<char> domain(domain_size);
    sid_size = static_cast<DWORD>(sid.size());
    if (LookupAccountNameA(nullptr, name_str.c_str(), sid.data(), &sid_size, domain.data(), &domain_size, &use) == 0)
        return std::nullopt;

    Id ret = Id::from_native(sid.data(), sid_size);
    if (!ret.valid())
        return std::nullopt;
    return ret;
}

template <typename Id>
std::optional<std::string> lookup_name(const Id & id)
{
    if (!id.valid())
        return std::nullopt;

    PSID sid = const_cast<unsigned char *>(id.native());
    DWORD name_size = 0;
    DWORD domain_size = 0;
    SID_NAME_USE use;

    // first call sizes both buffers
    LookupAccountSidA(nullptr, sid, nullptr, &name_size, nullptr, &domain_size, &use);
    if (name_size == 0)
        return std::nullopt;

    // + 1: room for the null terminator whatever the sizing call counted
    std::vector<char> name(name_size + 1);
    std::vector<char> domain(domain_size + 1);
    if (LookupAccountSidA(nullptr, sid, name.data(), &name_size, domain.data(), &domain_size, &use) == 0)
        return std::nullopt;

    return std::string(name.data());
}

UserId user_of(const IdentityToken & token)
{
    std::vector<BYTE> buffer = token.information(TokenUser);
    if (buffer.empty())
        return {};
    const TOKEN_USER *token_user = reinterpret_cast<const TOKEN_USER *>(buffer.data());
    return id_from_sid<UserId>(token_user->User.Sid);
}

GroupId group_of(const IdentityToken & token)
{
    std::vector<BYTE> buffer = token.information(TokenPrimaryGroup);
    if (buffer.empty())
        return {};
    const TOKEN_PRIMARY_GROUP *primary = reinterpret_cast<const TOKEN_PRIMARY_GROUP *>(buffer.data());
    return id_from_sid<GroupId>(primary->PrimaryGroup);
}

} // namespace

UserId effective_user()
{
    return user_of(IdentityToken::effective());
}

UserId real_user()
{
    return user_of(IdentityToken::process());
}

GroupId effective_group()
{
    return group_of(IdentityToken::effective());
}

GroupId real_group()
{
    return group_of(IdentityToken::process());
}

std::vector<GroupId> groups()
{
    const IdentityToken token = IdentityToken::effective();
    std::vector<BYTE> buffer = token.information(TokenGroups);
    if (buffer.empty())
        return {};

    const TOKEN_GROUPS *token_groups = reinterpret_cast<const TOKEN_GROUPS *>(buffer.data());
    std::vector<GroupId> ret;
    ret.reserve(token_groups->GroupCount);
    for (DWORD i = 0; i < token_groups->GroupCount; ++i)
    {
        // deny-only SIDs serve deny ACEs, not membership
        if ((token_groups->Groups[i].Attributes & SE_GROUP_USE_FOR_DENY_ONLY) != 0)
            continue;
        GroupId id = id_from_sid<GroupId>(token_groups->Groups[i].Sid);
        if (id.valid())
            ret.push_back(id);
    }
    return ret;
}

std::optional<std::string> name_of(const UserId & id)
{
    return lookup_name(id);
}

std::optional<std::string> name_of(const GroupId & id)
{
    return lookup_name(id);
}

std::optional<UserId> user_id_of(std::string_view user_name)
{
    return lookup_account<UserId>(user_name);
}

std::optional<GroupId> group_id_of(std::string_view group_name)
{
    return lookup_account<GroupId>(group_name);
}

std::optional<GroupId> primary_group_of(const UserId & id)
{
    // only the effective token exposes a primary group; other accounts would need a domain query
    if (!id.valid() || !(id == effective_user()))
        return std::nullopt;
    return effective_group();
}

std::optional<GroupId> primary_group_of(std::string_view user_name)
{
    const auto id = user_id_of(user_name);
    if (!id.has_value())
        return std::nullopt;
    return primary_group_of(*id);
}

bool is_root()
{
    const IdentityToken token = IdentityToken::effective();
    if (!token.valid())
        return false;
    TOKEN_ELEVATION elevation;
    DWORD size = 0;
    if (GetTokenInformation(token.handle(), TokenElevation, &elevation, sizeof(elevation), &size) == 0)
        return false;
    return elevation.TokenIsElevated != 0;
}

// windows has no setuid: a process cannot permanently become another account.
// see sihd::sys::Impersonation for the thread scoped, credential based counterpart.
bool drop_privileges([[maybe_unused]] const UserId & user_id, [[maybe_unused]] const GroupId & group_id)
{
    return false;
}

} // namespace sihd::sys::user
