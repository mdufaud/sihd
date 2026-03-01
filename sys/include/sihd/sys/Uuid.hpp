#ifndef __SIHD_SYS_UUID_HPP__
#define __SIHD_SYS_UUID_HPP__

#include <memory>
#include <string>
#include <string_view>

namespace sihd::sys
{

class Uuid
{
    public:
        Uuid();
        Uuid(std::string_view uuid_str);
        Uuid(const Uuid & uuid_namespace, std::string_view name);
        Uuid(const Uuid & other);
        ~Uuid();

        operator std::string() const { return this->str(); }
        operator bool() const { return !this->is_null(); }

        Uuid & operator=(const Uuid & other);
        bool operator==(const Uuid & other) const;

        void clear();

        bool is_null() const;
        std::string str() const;

        // RFC 4122 predefined namespaces
        constexpr static auto UUID_DNS = "6ba7b810-9dad-11d1-80b4-00c04fd430c8";
        constexpr static auto UUID_URL = "6ba7b811-9dad-11d1-80b4-00c04fd430c8";
        constexpr static auto UUID_OID = "6ba7b812-9dad-11d1-80b4-00c04fd430c8";
        constexpr static auto UUID_X500 = "6ba7b814-9dad-11d1-80b4-00c04fd430c8";

        static Uuid DNS() { return Uuid(UUID_DNS); }
        static Uuid URL() { return Uuid(UUID_URL); }
        static Uuid OID() { return Uuid(UUID_OID); }
        static Uuid X500() { return Uuid(UUID_X500); }

    protected:

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif