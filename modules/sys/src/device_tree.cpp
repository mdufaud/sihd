#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>

#include <sihd/sys/PluginLoader.hpp>
#include <sihd/sys/device_tree.hpp>
#include <sihd/util/type.hpp>

namespace sihd::sys::device_tree
{

using namespace sihd::util;
using sihd::core::Device;
using sihd::json::Json;

namespace
{

Result<void> check_keys(const Json & object, std::initializer_list<std::string_view> allowed, const std::string & where)
{
    if (object.is_object() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "{}: expected a json object", where));
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        if (std::ranges::find(allowed, std::string_view(it.key())) == allowed.end())
            return std::unexpected(Error(ErrorCode::invalid_argument, "{}: unknown key '{}'", where, it.key()));
    }
    return {};
}

Result<std::string> string_of(const Json & object, std::string_view key, const std::string & where)
{
    if (object.contains(key) == false || object[key].is_string() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "{}: '{}' must be a string", where, key));
    return object[key].get<std::string>();
}

Result<size_t> size_of(const Json & object, std::string_view key, size_t fallback, const std::string & where)
{
    if (object.contains(key) == false)
        return fallback;
    const Json value = object[key];
    if ((value.is_number_integer() == false && value.is_number_unsigned() == false) || value.get<int64_t>() < 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "{}: '{}' must be a positive integer", where, key));
    return static_cast<size_t>(value.get<int64_t>());
}

Result<void> add_channels(Device & device, const Json & channels)
{
    const std::string where = device.full_name();
    if (channels.is_array() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "{}: 'channels' must be an array", where));
    for (const Json & channel : channels)
    {
        if (auto res = check_keys(channel, {"name", "type", "size", "capacity"}, where); !res)
            return res;
        auto name = string_of(channel, "name", where);
        SIHD_UNEXPECTED_RETURN(name);
        auto type = string_of(channel, "type", where);
        SIHD_UNEXPECTED_RETURN(type);
        auto size = size_of(channel, "size", 1, where);
        SIHD_UNEXPECTED_RETURN(size);
        auto capacity = size_of(channel, "capacity", 0, where);
        SIHD_UNEXPECTED_RETURN(capacity);

        const Type data_type = type::from_str(*type);
        const bool added = data_type != TYPE_NONE
                           && (channel.contains("capacity")
                                   ? device.add_channel_resizable(*name, data_type, *size, *capacity) != nullptr
                                   : device.add_channel(*name, data_type, *size) != nullptr);
        if (added == false)
            return std::unexpected(
                Error(ErrorCode::invalid_argument, "{}: could not add channel '{}' of type '{}'", where, *name, *type));
    }
    return {};
}

Result<void> add_links(Device & device, const Json & links)
{
    const std::string where = device.full_name();
    if (links.is_object() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "{}: 'links' must be an object", where));
    for (auto it = links.begin(); it != links.end(); ++it)
    {
        if (it.value().is_string() == false)
            return std::unexpected(
                Error(ErrorCode::invalid_argument, "{}: link '{}' must be a path string", where, it.key()));
        if (device.add_link(it.key(), it.value().get<std::string>()) == false)
            return std::unexpected(Error(ErrorCode::invalid_argument, "{}: could not add link '{}'", where, it.key()));
    }
    return {};
}

Result<void> load_content(Device & device, const Json & content);

Result<void> add_devices(Device & parent, const Json & devices)
{
    const std::string where = parent.full_name();
    if (devices.is_array() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "{}: 'devices' must be an array", where));
    for (const Json & description : devices)
    {
        if (auto res = check_keys(description, {"class", "name", "conf", "channels", "links", "devices"}, where); !res)
            return res;
        auto classname = string_of(description, "class", where);
        SIHD_UNEXPECTED_RETURN(classname);
        auto name = string_of(description, "name", where);
        SIHD_UNEXPECTED_RETURN(name);
        // the constructor throws on a name already taken
        if (parent.get_child(*name) != nullptr)
            return std::unexpected(Error(ErrorCode::already_exists, "{}: child '{}' already exists", where, *name));

        auto created = PluginLoader::create_from_linked(*classname, *name, &parent);
        SIHD_UNEXPECTED_RETURN(created);
        Device *device = dynamic_cast<Device *>(*created);
        if (device == nullptr)
            return std::unexpected(
                Error(ErrorCode::invalid_argument, "{}: class '{}' is not a device", where, *classname));
        if (description.contains("conf"))
            if (auto res = device->set_conf(description["conf"]); !res)
                return res;
        if (auto res = load_content(*device, description); !res)
            return res;
    }
    return {};
}

Result<void> load_content(Device & device, const Json & content)
{
    if (content.contains("channels"))
        if (auto res = add_channels(device, content["channels"]); !res)
            return res;
    if (content.contains("links"))
        if (auto res = add_links(device, content["links"]); !res)
            return res;
    if (content.contains("devices"))
        if (auto res = add_devices(device, content["devices"]); !res)
            return res;
    return {};
}

} // namespace

Result<void> load(Device & parent, const Json & tree)
{
    if (auto res = check_keys(tree, {"channels", "links", "devices"}, parent.full_name()); !res)
        return res;
    return load_content(parent, tree);
}

} // namespace sihd::sys::device_tree
