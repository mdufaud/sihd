#include <bit>
#include <cstring>

#include <sihd/sys/Uuid.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/hash.hpp>

#if defined(__SIHD_WINDOWS__)
# include <rpc.h>
#else
# include <uuid/uuid.h>
#endif

namespace sihd::sys
{

using namespace sihd::util;

namespace
{

void do_copy(uuid_t & uuid_to, const uuid_t & uuid_from)
{
#if defined(__SIHD_WINDOWS__)
    memcpy(&uuid_to, &uuid_from, sizeof(uuid_t));
#else
    uuid_copy(uuid_to, uuid_from);
#endif
}

void do_clear(uuid_t & uuid)
{
#if defined(__SIHD_WINDOWS__)
    memset(&uuid, 0, sizeof(uuid_t));
#else
    memset(uuid, 0, sizeof(uuid_t));
#endif
}

} // namespace

struct Uuid::Impl
{
        Impl() { do_clear(uuid); }
        ~Impl() = default;

        uuid_t uuid;
};

Uuid::Uuid()
{
    _impl = std::make_unique<Impl>();
#if defined(__SIHD_WINDOWS__)
    UuidCreate(&_impl->uuid);
#else
    uuid_generate(_impl->uuid);
#endif
}

Uuid::Uuid(const std::string & uuid_str)
{
    _impl = std::make_unique<Impl>();
#if defined(__SIHD_WINDOWS__)
    if (UuidFromString((unsigned char *)uuid_str.c_str(), &_impl->uuid) != RPC_S_OK)
        do_clear(_impl->uuid);
#else
    if (uuid_parse(uuid_str.c_str(), _impl->uuid) != 0)
        do_clear(_impl->uuid);
#endif
}

Uuid::Uuid(const Uuid & uuid_namespace, std::string_view name)
{
    if (uuid_namespace.is_null())
        throw std::invalid_argument("Namespace uuid is null");

    _impl = std::make_unique<Impl>();

    // Build input buffer = namespace UUID + name
    std::vector<unsigned char> data;
    data.reserve(16 + name.size());

#if defined(__SIHD_WINDOWS__)
    // GUID fields are little-endian: byteswap each one, the memory then holds network order
    GUID le;
    std::memcpy(&le, &uuid_namespace._impl->uuid, sizeof(le));
    le.Data1 = std::byteswap(le.Data1);
    le.Data2 = std::byteswap(le.Data2);
    le.Data3 = std::byteswap(le.Data3);
    const auto *bytes = reinterpret_cast<const unsigned char *>(&le);
    data.insert(data.end(), bytes, bytes + sizeof(le));
#else
    // UUID already in network byte order
    data.insert(data.end(),
                (unsigned char *)uuid_namespace._impl->uuid,
                (unsigned char *)uuid_namespace._impl->uuid + 16);
#endif

    // Append the name bytes
    data.insert(data.end(), name.begin(), name.end());

    // Compute SHA-1
    unsigned char hash[20];
    hash::sha1(data.data(), data.size(), hash);

    // Set Version and Variant bits
    hash[6] = (hash[6] & 0x0F) | 0x50; // version 5
    hash[8] = (hash[8] & 0x3F) | 0x80; // variant RFC4122

    // Write the result into the uuid structure
#if defined(__SIHD_WINDOWS__)
    GUID *guid_ptr = (GUID *)(&_impl->uuid);
    // hash is in network order: the fields load little-endian, byteswap them back
    std::memcpy(guid_ptr, hash, 16);
    guid_ptr->Data1 = std::byteswap(guid_ptr->Data1);
    guid_ptr->Data2 = std::byteswap(guid_ptr->Data2);
    guid_ptr->Data3 = std::byteswap(guid_ptr->Data3);
#else
    memcpy(_impl->uuid, hash, 16);
#endif
}

Uuid::Uuid(const Uuid & other)
{
    _impl = std::make_unique<Impl>();
    do_copy(_impl->uuid, other._impl->uuid);
}

Uuid::~Uuid() = default;

Uuid & Uuid::operator=(const Uuid & other)
{
    do_copy(_impl->uuid, other._impl->uuid);
    return *this;
}

bool Uuid::operator==(const Uuid & other) const
{
#if defined(__SIHD_WINDOWS__)
    RPC_STATUS status;
    return UuidCompare(const_cast<uuid_t *>(&_impl->uuid), const_cast<uuid_t *>(&other._impl->uuid), &status) == 0
           && status == RPC_S_OK;
#else
    return uuid_compare(_impl->uuid, other._impl->uuid) == 0;
#endif
}

bool Uuid::is_null() const
{
#if defined(__SIHD_WINDOWS__)
    RPC_STATUS status;
    return UuidIsNil(const_cast<uuid_t *>(&_impl->uuid), &status) == TRUE;
#else
    return uuid_is_null(_impl->uuid) == 1;
#endif
}

std::string Uuid::str() const
{
    std::string ret;
#if defined(__SIHD_WINDOWS__)
    unsigned char *out;
    if (UuidToString(const_cast<uuid_t *>(&_impl->uuid), &out) == RPC_S_OK)
    {
        ret = (char *)out;
        RpcStringFree(&out);
    }
#else
    char out[36 + 1];
    uuid_unparse(_impl->uuid, out);
    ret = out;
#endif
    return ret;
}

void Uuid::clear()
{
    do_clear(_impl->uuid);
}

} // namespace sihd::sys