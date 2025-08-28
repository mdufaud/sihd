#include <sihd/util/Logger.hpp>
#include <sihd/util/Uuid.hpp>

#include <sihd/util/platform.hpp>

#if defined(__SIHD_WINDOWS__)
# include <rpc.h>
#else
# include <uuid/uuid.h>
#endif

namespace sihd::util
{

namespace
{

void do_copy(uuid_t uuid_to, const uuid_t uuid_from)
{
#if defined(__SIHD_WINDOWS__)
    memcpy(uuid_to, uuid_from, sizeof(uuid_t));
#else
    uuid_copy(uuid_to, uuid_from);
#endif
}

void do_clear(uuid_t uuid)
{
    memset(uuid, 0, sizeof(uuid_t));
}

} // namespace

Uuid::Uuid()
{
    this->clear();
#if defined(__SIHD_WINDOWS__)
    UuidCreate(&_uuid);
#else
    uuid_generate(_uuid);
#endif
}

Uuid::Uuid(std::string_view uuid_str)
{
    this->clear();
#if defined(__SIHD_WINDOWS__)
    UuidFromString((unsigned char *)uuid_str.data(), &_uuid);
#else
    uuid_parse(uuid_str.data(), _uuid);
#endif
}

Uuid::Uuid(const Uuid & uuid_namespace, std::string_view name)
{
    if (uuid_namespace.is_null())
        throw std::invalid_argument("Namespace uuid is null");

    this->clear();

#if defined(__SIHD_WINDOWS__)
    // 1. Build input buffer = namespace UUID (big endian) + name
    std::vector<unsigned char> data;
    data.reserve(16 + name.size());

    // Convert namespace (which is also an unsigned char[16]) into big-endian order
    data.insert(data.end(), uuid_namespace._uuid, uuid_namespace._uuid + 16);

    // Append the name bytes
    data.insert(data.end(), name.begin(), name.end());

    // 2. Compute SHA-1
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[20];
    DWORD hashLen = sizeof(hash);

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        throw std::runtime_error("CryptAcquireContext failed");

    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
    {
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("CryptCreateHash failed");
    }

    if (!CryptHashData(hHash, data.data(), (DWORD)data.size(), 0))
    {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("CryptHashData failed");
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0))
    {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("CryptGetHashParam failed");
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    // 3. Copy first 16 bytes of SHA1 into _uuid
    memcpy(_uuid, hash, 16);

    // 4. Set version (5) and variant (RFC 4122)
    _uuid[6] = (_uuid[6] & 0x0F) | (5 << 4); // version 5
    _uuid[8] = (_uuid[8] & 0x3F) | 0x80;     // variant RFC4122
#else
    uuid_generate_sha1(_uuid, uuid_namespace._uuid, name.data(), name.size());
#endif
}

Uuid::Uuid(const Uuid & other)
{
    do_copy(_uuid, other._uuid);
}

Uuid::~Uuid() = default;

Uuid & Uuid::operator=(const Uuid & other)
{
    do_copy(_uuid, other._uuid);
    return *this;
}

bool Uuid::operator==(const Uuid & other) const
{
#if defined(__SIHD_WINDOWS__)
    RPC_STATUS status;
    return UuidCompare(const_cast<uuid_t *>(&_uuid), const_cast<uuid_t *>(other._uuid), &status) == 0
           && status == RPC_S_OK;
#else
    return uuid_compare(_uuid, other._uuid) == 0;
#endif
}

bool Uuid::is_null() const
{
#if defined(__SIHD_WINDOWS__)
    RPC_STATUS status;
    return UuidIsNil(const_cast<uuid_t *>(&_uuid), &status) == TRUE;
#else
    return uuid_is_null(_uuid) == 1;
#endif
}

std::string Uuid::str() const
{
    std::string ret;
#if defined(__SIHD_WINDOWS__)
    unsigned char *out;
    if (UuidToString(const_cast<uuid_t *>(&_uuid), &out) == RPC_S_OK)
    {
        ret = (char *)out;
        RpcStringFree(&out);
    }
#else
    char out[36 + 1];
    uuid_unparse(_uuid, out);
    ret = out;
#endif
    return ret;
}

void Uuid::clear()
{
    do_clear(_uuid);
}

} // namespace sihd::util