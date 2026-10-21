#include <windows.h>

#include <utility>

#include <sihd/sys/windows/registry.hpp>
#include <sihd/util/str.hpp>

#include "internal/win_error.hpp"

using namespace sihd::util;
using sihd::sys::internal::Win32Error;

namespace sihd::sys::windows_::registry
{

namespace
{

struct KeyInfo
{
        DWORD subkey_count = 0;
        DWORD subkey_max_length = 0;
        DWORD value_count = 0;
        DWORD value_max_length = 0;
};

using EnumerateName = LSTATUS (*)(HKEY key, DWORD index, char *name, DWORD *length);

HKEY native_hive(Hive hive)
{
    switch (hive)
    {
        case Hive::classes_root:
            return HKEY_CLASSES_ROOT;
        case Hive::current_user:
            return HKEY_CURRENT_USER;
        case Hive::local_machine:
            return HKEY_LOCAL_MACHINE;
        case Hive::users:
            return HKEY_USERS;
        case Hive::current_config:
            return HKEY_CURRENT_CONFIG;
    }
    return nullptr;
}

Result<std::string> read_value(HKEY key, const std::string & name, DWORD flags)
{
    DWORD size = 0;
    const Win32Error sized {RegGetValueA(key, nullptr, name.c_str(), flags, nullptr, nullptr, &size)};
    if (sized.failed())
        return std::unexpected(Error(sized, "could not read registry value '{}': {}", name, sized));

    std::string data(size, '\0');
    const Win32Error read {RegGetValueA(key, nullptr, name.c_str(), flags, nullptr, data.data(), &size)};
    if (read.failed())
        return std::unexpected(Error(read, "could not read registry value '{}': {}", name, read));
    data.resize(size);
    return data;
}

Result<KeyInfo> query_info(HKEY key)
{
    KeyInfo info;
    const Win32Error queried {RegQueryInfoKeyA(key,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               &info.subkey_count,
                                               &info.subkey_max_length,
                                               nullptr,
                                               &info.value_count,
                                               &info.value_max_length,
                                               nullptr,
                                               nullptr,
                                               nullptr)};
    if (queried.failed())
        return std::unexpected(Error(queried, "could not query registry key: {}", queried));
    return info;
}

LSTATUS enumerate_subkey(HKEY key, DWORD index, char *name, DWORD *length)
{
    return RegEnumKeyExA(key, index, name, length, nullptr, nullptr, nullptr, nullptr);
}

LSTATUS enumerate_value(HKEY key, DWORD index, char *name, DWORD *length)
{
    return RegEnumValueA(key, index, name, length, nullptr, nullptr, nullptr, nullptr);
}

Result<std::vector<std::string>> list_names(HKEY key, DWORD count, DWORD max_length, EnumerateName enumerate)
{
    std::string buffer(max_length + 1, '\0');
    std::vector<std::string> names;
    names.reserve(count);
    for (DWORD i = 0; i < count; ++i)
    {
        DWORD length = static_cast<DWORD>(buffer.size());
        const Win32Error status {enumerate(key, i, buffer.data(), &length)};
        if (status.failed())
            return std::unexpected(Error(status, "could not enumerate registry key: {}", status));
        names.emplace_back(buffer.data(), length);
    }
    return names;
}

template <typename T>
Result<T> read_scalar(HKEY key, const std::string & name, DWORD flags)
{
    T value = 0;
    DWORD size = sizeof(value);
    const Win32Error status {RegGetValueA(key, nullptr, name.c_str(), flags, nullptr, &value, &size)};
    if (status.failed())
        return std::unexpected(Error(status, "could not read registry value '{}': {}", name, status));
    return value;
}

Result<void> write_value(HKEY key, const std::string & name, DWORD type, const void *data, size_t size)
{
    const Win32Error status {RegSetKeyValueA(key, nullptr, name.c_str(), type, data, static_cast<DWORD>(size))};
    if (status.failed())
        return std::unexpected(Error(status, "could not write registry value '{}': {}", name, status));
    return {};
}

} // namespace

struct Key::Impl
{
        ~Impl() { this->close(); }

        void close()
        {
            if (key != nullptr)
            {
                RegCloseKey(key);
                key = nullptr;
            }
        }

        HKEY key = nullptr;
};

Key::Key(): _impl(std::make_unique<Impl>()) {}

Key::Key(Key && other) = default;

Key & Key::operator=(Key && other) = default;

Key::~Key() = default;

Result<void> Key::open(Hive hive, const std::string & path, bool writable)
{
    this->close();
    const REGSAM access = writable ? KEY_READ | KEY_WRITE : KEY_READ;
    const Win32Error status {RegOpenKeyExA(native_hive(hive), path.c_str(), 0, access, &_impl->key)};
    if (status.failed())
    {
        _impl->key = nullptr;
        return std::unexpected(Error(status, "could not open registry key '{}': {}", path, status));
    }
    return {};
}

Result<void> Key::create(Hive hive, const std::string & path)
{
    this->close();
    const Win32Error status {RegCreateKeyExA(native_hive(hive),
                                             path.c_str(),
                                             0,
                                             nullptr,
                                             REG_OPTION_NON_VOLATILE,
                                             KEY_READ | KEY_WRITE,
                                             nullptr,
                                             &_impl->key,
                                             nullptr)};
    if (status.failed())
    {
        _impl->key = nullptr;
        return std::unexpected(Error(status, "could not create registry key '{}': {}", path, status));
    }
    return {};
}

void Key::close()
{
    _impl->close();
}

bool Key::is_open() const
{
    return _impl != nullptr && _impl->key != nullptr;
}

Result<std::string> Key::string(const std::string & name) const
{
    auto data = read_value(_impl->key, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND);
    SIHD_UNEXPECTED_RETURN(data);
    return std::string(data->c_str());
}

Result<std::vector<std::string>> Key::strings(const std::string & name) const
{
    auto data = read_value(_impl->key, name, RRF_RT_REG_MULTI_SZ);
    SIHD_UNEXPECTED_RETURN(data);
    const size_t end = data->find_last_not_of('\0');
    if (end == std::string::npos)
        return std::vector<std::string> {};
    data->resize(end + 1);
    return str::split(*data, '\0');
}

Result<uint32_t> Key::u32(const std::string & name) const
{
    return read_scalar<uint32_t>(_impl->key, name, RRF_RT_REG_DWORD);
}

Result<uint64_t> Key::u64(const std::string & name) const
{
    return read_scalar<uint64_t>(_impl->key, name, RRF_RT_REG_QWORD);
}

Result<std::string> Key::binary(const std::string & name) const
{
    return read_value(_impl->key, name, RRF_RT_REG_BINARY);
}

Result<void> Key::set_string(const std::string & name, const std::string & value)
{
    return write_value(_impl->key, name, REG_SZ, value.c_str(), value.size() + 1);
}

Result<void> Key::set_strings(const std::string & name, const std::vector<std::string> & values)
{
    std::string data = str::join(values, std::string_view("\0", 1));
    data.append(2, '\0');
    return write_value(_impl->key, name, REG_MULTI_SZ, data.data(), data.size());
}

Result<void> Key::set_u32(const std::string & name, uint32_t value)
{
    return write_value(_impl->key, name, REG_DWORD, &value, sizeof(value));
}

Result<void> Key::set_u64(const std::string & name, uint64_t value)
{
    return write_value(_impl->key, name, REG_QWORD, &value, sizeof(value));
}

Result<void> Key::set_binary(const std::string & name, ArrByteView value)
{
    return write_value(_impl->key, name, REG_BINARY, value.data(), value.byte_size());
}

Result<void> Key::remove_value(const std::string & name)
{
    const Win32Error status {RegDeleteValueA(_impl->key, name.c_str())};
    if (status.failed())
        return std::unexpected(Error(status, "could not remove registry value '{}': {}", name, status));
    return {};
}

Result<std::vector<std::string>> Key::subkeys() const
{
    auto info = query_info(_impl->key);
    SIHD_UNEXPECTED_RETURN(info);
    return list_names(_impl->key, info->subkey_count, info->subkey_max_length, enumerate_subkey);
}

Result<std::vector<std::string>> Key::value_names() const
{
    auto info = query_info(_impl->key);
    SIHD_UNEXPECTED_RETURN(info);
    return list_names(_impl->key, info->value_count, info->value_max_length, enumerate_value);
}

Result<void> remove(Hive hive, const std::string & path)
{
    const Win32Error status {RegDeleteTreeA(native_hive(hive), path.c_str())};
    if (status.failed())
        return std::unexpected(Error(status, "could not remove registry key '{}': {}", path, status));
    return {};
}

} // namespace sihd::sys::windows_::registry
