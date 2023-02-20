#ifndef __SIHD_UTIL_UUID_HPP__
#define __SIHD_UTIL_UUID_HPP__

#include <string_view>

#include <sihd/util/platform.hpp>

#if defined(__SIHD_WINDOWS__)
# include <rpc.h>
#else
# include <uuid/uuid.h>
#endif

namespace sihd::util
{

class Uuid
{
    public:
        Uuid();
        Uuid(std::string_view s);
        Uuid(const uuid_t *uuid);
        Uuid(const Uuid & other);
        ~Uuid();

        operator bool() const { return !this->is_null(); }

        Uuid & operator=(const Uuid & other);
        bool operator==(const Uuid & other) const;

        bool is_null() const;
        std::string str() const;
        const uuid_t *uuid() const { return &_uuid; }

    protected:

    private:
        void _clear();
        void _copy(const uuid_t *uuid);

        uuid_t _uuid;
};

} // namespace sihd::util

#endif