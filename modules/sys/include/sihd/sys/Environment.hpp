#ifndef __SIHD_SYS_ENVIRONMENT_HPP__
#define __SIHD_SYS_ENVIRONMENT_HPP__

#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace sihd::sys
{

// in-memory key/value environment, distinct from the process-wide one (sihd::sys::env)
class Environment
{
    public:
        // snapshot of the current process environment
        static Environment from_current();

        // loads "KEY=VALUE" entries; entries without '=' are ignored
        void load(std::span<std::string_view> entries);
        void load(std::span<const std::string> entries);
        void load(std::span<const char *> entries);
        void load(std::initializer_list<std::string_view> entries);

        void set(std::string_view key, std::string_view value);
        std::optional<std::string> get(std::string_view key) const;
        bool rm(std::string_view key);
        void clear();

        bool empty() const;
        size_t size() const;
        const std::map<std::string, std::string> & entries() const;

        // replaces $VAR and ${VAR} with this environment's values;
        // unknown or malformed references are left as-is
        std::string expand(std::string_view str) const;

    private:
        std::map<std::string, std::string> _entries;
};

} // namespace sihd::sys

#endif
