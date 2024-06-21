#include <sihd/util/Logger.hpp>
#include <sihd/util/Uuid.hpp>

#include <sihd/util/platform.hpp>

#if defined(__SIHD_WINDOWS__)
# include <rpc.h>
#else
# if defined __has_include
#  if __has_include(<uuid.h>)
#   include <uuid.h>
#  elif __has_include(<uuid/uuid.h>)
#   include <uuid/uuid.h>
#  endif
# else
#  include <uuid.h>
# endif
#endif

namespace sihd::util
{

Uuid::Uuid()
{
    this->_clear();
#if defined(__SIHD_WINDOWS__)
    UuidCreate(&_uuid);
#else
    uuid_generate(_uuid);
#endif
}

Uuid::Uuid(std::string_view s)
{
    this->_clear();
#if defined(__SIHD_WINDOWS__)
    UuidFromString((unsigned char *)s.data(), &_uuid);
#else
    uuid_parse(s.data(), _uuid);
#endif
}

Uuid::Uuid(const uuid_t *uuid)
{
    this->_copy(uuid);
}

Uuid::Uuid(const Uuid & other): Uuid(other.uuid()) {}

Uuid::~Uuid() {}

Uuid & Uuid::operator=(const Uuid & other)
{
    this->_copy(other.uuid());
    return *this;
}

bool Uuid::operator==(const Uuid & other) const
{
#if defined(__SIHD_WINDOWS__)
    RPC_STATUS status;
    return UuidCompare(const_cast<uuid_t *>(&_uuid), const_cast<uuid_t *>(other.uuid()), &status) == 0
           && status == RPC_S_OK;
#else
    return uuid_compare(_uuid, *other.uuid()) == 0;
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

void Uuid::_copy(const uuid_t *uuid)
{
    this->_clear();
#if defined(__SIHD_WINDOWS__)
    memcpy(&_uuid, uuid, sizeof(uuid_t));
#else
    uuid_copy(_uuid, *uuid);
#endif
}

void Uuid::_clear()
{
#if defined(__SIHD_WINDOWS__)
    memset(&_uuid, 0, sizeof(uuid_t));
#else
    memset(_uuid, 0, sizeof(uuid_t));
#endif
}

} // namespace sihd::util