#include <sihd/bt/adapter.hpp>

#include "java.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace
{

// Adapters.adapterInfo(): address, name, enabled, discoverable, discovering
Result<AdapterInfo> adapter_info()
{
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());
    jmethodID info = env->GetStaticMethodID(runtime->classes.adapters,
                                            "adapterInfo",
                                            "(Landroid/content/Context;)[Ljava/lang/String;");
    auto fields = static_cast<jobjectArray>(
        env->CallStaticObjectMethod(runtime->classes.adapters, info, runtime->context));
    auto read = java::check(env.get(), "bluetooth adapter");
    SIHD_UNEXPECTED_RETURN(read);
    const std::vector<std::string> values = java::strings(env.get(), fields);
    env->DeleteLocalRef(fields);
    const bool enabled = values.at(2) == "1";
    return AdapterInfo {
        .identifier = "default",
        .address = values.at(0),
        .name = values.at(1),
        .alias = values.at(1),
        .powered = enabled,
        .discoverable = values.at(3) == "1",
        .pairable = enabled,
        .discovering = values.at(4) == "1",
    };
}

Result<void> check_index(size_t index)
{
    if (index != 0)
        return std::unexpected(Error(not_found, "no bluetooth adapter at index {}", index));
    return {};
}

Error system_only(const char *what)
{
    return Error(not_supported, "android lets only the user change {}", what);
}

} // namespace

Result<bool> bluetooth_enabled()
{
    auto info = adapter_info();
    SIHD_UNEXPECTED_RETURN(info);
    return info->powered;
}

Result<std::vector<AdapterInfo>> list_adapters()
{
    auto info = adapter_info();
    if (!info && info.error().code == not_supported)
        return std::vector<AdapterInfo> {};
    SIHD_UNEXPECTED_RETURN(info);
    return std::vector<AdapterInfo> {std::move(*info)};
}

Result<void> set_adapter_powered(size_t index, bool)
{
    auto known = check_index(index);
    SIHD_UNEXPECTED_RETURN(known);
    return std::unexpected(system_only("the bluetooth power"));
}

Result<void> set_adapter_discoverable(size_t index, bool)
{
    auto known = check_index(index);
    SIHD_UNEXPECTED_RETURN(known);
    return std::unexpected(system_only("the discoverability"));
}

Result<void> set_adapter_pairable(size_t index, bool)
{
    auto known = check_index(index);
    SIHD_UNEXPECTED_RETURN(known);
    return std::unexpected(system_only("whether pairings are accepted"));
}

Result<void> set_adapter_alias(size_t index, const std::string & alias)
{
    auto known = check_index(index);
    SIHD_UNEXPECTED_RETURN(known);
    auto runtime = java::runtime();
    SIHD_UNEXPECTED_RETURN(runtime);
    java::Env env(runtime->vm);
    if (!env)
        return std::unexpected(java::detached());
    jmethodID set_name = env->GetStaticMethodID(runtime->classes.adapters,
                                                "setName",
                                                "(Landroid/content/Context;Ljava/lang/String;)V");
    jstring name = java::jstring_of(env.get(), alias);
    env->CallStaticVoidMethod(runtime->classes.adapters, set_name, runtime->context, name);
    env->DeleteLocalRef(name);
    return java::check(env.get(), "bluetooth adapter name");
}

} // namespace sihd::bt
