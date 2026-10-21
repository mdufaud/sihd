#ifndef __SIHD_SYS_WINDOWS_REGISTRY_HPP__
#define __SIHD_SYS_WINDOWS_REGISTRY_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# error "sihd/sys/windows headers are windows only"
#endif

namespace sihd::sys::windows_::registry
{

enum class Hive
{
    classes_root,
    current_user,
    local_machine,
    users,
    current_config,
};

class Key
{
    public:
        Key();
        Key(Key && other);
        Key & operator=(Key && other);
        ~Key();

        Key(const Key &) = delete;
        Key & operator=(const Key &) = delete;

        sihd::util::Result<void> open(Hive hive, const std::string & path, bool writable = false);
        sihd::util::Result<void> create(Hive hive, const std::string & path);
        void close();

        bool is_open() const;

        // expand strings are read unexpanded
        sihd::util::Result<std::string> string(const std::string & name) const;
        sihd::util::Result<std::vector<std::string>> strings(const std::string & name) const;
        sihd::util::Result<uint32_t> u32(const std::string & name) const;
        sihd::util::Result<uint64_t> u64(const std::string & name) const;
        sihd::util::Result<std::string> binary(const std::string & name) const;

        sihd::util::Result<void> set_string(const std::string & name, const std::string & value);
        sihd::util::Result<void> set_strings(const std::string & name, const std::vector<std::string> & values);
        sihd::util::Result<void> set_u32(const std::string & name, uint32_t value);
        sihd::util::Result<void> set_u64(const std::string & name, uint64_t value);
        sihd::util::Result<void> set_binary(const std::string & name, sihd::util::ArrByteView value);

        sihd::util::Result<void> remove_value(const std::string & name);

        sihd::util::Result<std::vector<std::string>> subkeys() const;
        sihd::util::Result<std::vector<std::string>> value_names() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

sihd::util::Result<void> remove(Hive hive, const std::string & path);

} // namespace sihd::sys::windows_::registry

#endif
