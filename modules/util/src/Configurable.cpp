#include <sihd/json/Json.hpp>
#include <sihd/util/Configurable.hpp>

namespace sihd::util
{

std::unexpected<Error> Configurable::_no_conf_key(const std::string & name)
{
    return std::unexpected(Error(ErrorCode::not_found, "no conf key '{}'", name));
}

std::expected<void, Error> Configurable::set_conf_str(const std::string & name, const char *param)
{
    std::string_view str_view(param);
    return this->set_conf_str(name, str_view);
}

std::expected<void, Error> Configurable::set_conf(const sihd::json::Json & json)
{
    if (json.is_null() || json.is_object() == false)
        return std::unexpected(Error(ErrorCode::invalid_argument, "conf is not a json object"));
    for (auto it = json.begin(); it != json.end(); ++it)
    {
        std::expected<void, Error> res = this->_set_conf_from_json(it.key(), it.value());
        if (!res)
            return res;
    }
    return {};
}

std::expected<void, Error> Configurable::set_conf(const std::string & key, const sihd::json::Json & val)
{
    return this->_set_conf_from_json(key, val);
}

std::expected<void, Error> Configurable::_set_conf_from_json(const std::string & key, const sihd::json::Json & val)
{
    if (val.is_object())
        return this->_set_conf_json(key, val);
    if (val.is_array())
    {
        for (const auto & elem : val)
        {
            std::expected<void, Error> res = this->_set_conf_from_json(key, elem);
            if (!res)
                return res;
        }
        return {};
    }
    if (val.is_number_integer() || val.is_number_unsigned())
        return this->set_conf_int(key, val.get<int64_t>());
    if (val.is_number_float())
        return this->set_conf_float(key, val.get<double>());
    if (val.is_string())
        return this->set_conf_str(key, val.get<std::string>());
    if (val.is_bool())
        return this->set_conf<bool>(key, val.get<bool>());
    return std::unexpected(Error(ErrorCode::invalid_argument, "conf '{}': unsupported value", key));
}

std::expected<void, Error> Configurable::_set_conf_json(const std::string & name, const sihd::json::Json & val)
{
    if (_callback_manager.exists(name) == false)
        return this->_no_conf_key(name);
    if (_callback_manager.check_call_type<bool, const sihd::json::Json &>(name))
        return this->_call_conf<const sihd::json::Json &>(name, val);
    if (_callback_manager.check_call_type<bool, sihd::json::Json>(name))
        return this->_call_conf<sihd::json::Json>(name, val);
    return std::unexpected(Error(ErrorCode::not_supported, "conf '{}': no handler for this value", name));
}

std::expected<void, Error> Configurable::set_conf_float(const std::string & name, double param)
{
    if (_callback_manager.exists(name) == false)
        return this->_no_conf_key(name);
    if (_callback_manager.check_call_type<bool, double>(name))
        return this->_call_conf<double>(name, param);
    if (_callback_manager.check_call_type<bool, float>(name))
        return this->_call_conf<float>(name, static_cast<float>(param));
    return std::unexpected(Error(ErrorCode::not_supported, "conf '{}': no handler for this value", name));
}

std::expected<void, Error> Configurable::set_conf_int(const std::string & name, int64_t param)
{
    if (_callback_manager.exists(name) == false)
        return this->_no_conf_key(name);
    if (_callback_manager.check_call_type<bool, int64_t>(name))
        return this->_call_conf<int64_t>(name, param);
    if (_callback_manager.check_call_type<bool, uint64_t>(name))
        return this->_call_conf<uint64_t>(name, static_cast<uint64_t>(param));
    if (_callback_manager.check_call_type<bool, int32_t>(name))
        return this->_call_conf<int32_t>(name, static_cast<int32_t>(param));
    if (_callback_manager.check_call_type<bool, uint32_t>(name))
        return this->_call_conf<uint32_t>(name, static_cast<uint32_t>(param));
    if (_callback_manager.check_call_type<bool, int16_t>(name))
        return this->_call_conf<int16_t>(name, static_cast<int16_t>(param));
    if (_callback_manager.check_call_type<bool, uint16_t>(name))
        return this->_call_conf<uint16_t>(name, static_cast<uint16_t>(param));
    if (_callback_manager.check_call_type<bool, int8_t>(name))
        return this->_call_conf<int8_t>(name, static_cast<int8_t>(param));
    if (_callback_manager.check_call_type<bool, uint8_t>(name))
        return this->_call_conf<uint8_t>(name, static_cast<uint8_t>(param));
    return std::unexpected(Error(ErrorCode::not_supported, "conf '{}': no handler for this value", name));
}

std::expected<void, Error> Configurable::set_conf_str(const std::string & name, const std::string & param)
{
    if (_callback_manager.exists(name) == false)
        return this->_no_conf_key(name);
    if (_callback_manager.check_call_type<bool, std::string>(name))
        return this->_call_conf<std::string>(name, param);
    if (_callback_manager.check_call_type<bool, const std::string &>(name))
        return this->_call_conf<const std::string &>(name, param);
    if (_callback_manager.check_call_type<bool, std::string_view>(name))
        return this->_call_conf<std::string_view>(name, param);
    if (_callback_manager.check_call_type<bool, const char *>(name))
        return this->_call_conf<const char *>(name, param.c_str());
    return std::unexpected(Error(ErrorCode::not_supported, "conf '{}': no handler for this value", name));
}

std::expected<void, Error> Configurable::set_conf_str(const std::string & name, std::string_view param)
{
    return this->set_conf_str(name, std::string(param));
}

} // namespace sihd::util
