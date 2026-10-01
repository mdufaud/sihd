#include <sihd/json/Json.hpp>
#include <sihd/util/Configurable.hpp>

namespace sihd::util
{

bool Configurable::set_conf_str(const std::string & name, const char *param)
{
    std::string_view str_view(param);
    return this->set_conf_str(name, str_view);
}

bool Configurable::set_conf(const sihd::json::Json & json)
{
    if (json.is_null() || !json.is_object())
        return false;
    bool ret = true;
    for (auto it = json.begin(); it != json.end(); ++it)
    {
        if (!this->_set_conf_from_json(it.key(), it.value()))
            ret = false;
    }
    return ret;
}

bool Configurable::set_conf(const std::string & key, const sihd::json::Json & val)
{
    return this->_set_conf_from_json(key, val);
}

bool Configurable::_set_conf_from_json(const std::string & key, const sihd::json::Json & val)
{
    if (val.is_null())
        return false;
    if (val.is_object())
        return this->_set_conf_json(key, val);
    if (val.is_array())
    {
        bool ret = true;
        for (const auto & elem : val)
        {
            if (!this->_set_conf_from_json(key, elem))
                ret = false;
        }
        return ret;
    }
    if (val.is_number_integer())
        return this->set_conf_int(key, val.get<int64_t>());
    if (val.is_number_unsigned())
        return this->set_conf_int(key, static_cast<int64_t>(val.get<uint64_t>()));
    if (val.is_number_float())
        return this->set_conf_float(key, val.get<double>());
    if (val.is_string())
        return this->set_conf_str(key, val.get<std::string>());
    if (val.is_bool())
        return this->set_conf(key, val.get<bool>());
    return false;
}

bool Configurable::_set_conf_json(const std::string & name, const sihd::json::Json & val)
{
    if (_callback_manager.check_call_type<bool, const sihd::json::Json &>(name))
        return _callback_manager.call<bool, const sihd::json::Json &>(name, val);
    return _callback_manager.call<bool, sihd::json::Json>(name, val);
}

bool Configurable::set_conf_float(const std::string & name, double param)
{
    if (_callback_manager.check_call_type<bool, double>(name))
        return _callback_manager.call<bool, double>(name, param);
    return _callback_manager.call<bool, float>(name, static_cast<float>(param));
}

bool Configurable::set_conf_int(const std::string & name, int64_t param)
{
    if (_callback_manager.check_call_type<bool, int64_t>(name))
        return _callback_manager.call<bool, int64_t>(name, param);
    if (_callback_manager.check_call_type<bool, uint64_t>(name))
        return _callback_manager.call<bool, uint64_t>(name, static_cast<uint64_t>(param));
    if (_callback_manager.check_call_type<bool, int32_t>(name))
        return _callback_manager.call<bool, int32_t>(name, static_cast<int32_t>(param));
    if (_callback_manager.check_call_type<bool, uint32_t>(name))
        return _callback_manager.call<bool, uint32_t>(name, static_cast<uint32_t>(param));
    if (_callback_manager.check_call_type<bool, int16_t>(name))
        return _callback_manager.call<bool, int16_t>(name, static_cast<int16_t>(param));
    if (_callback_manager.check_call_type<bool, uint16_t>(name))
        return _callback_manager.call<bool, uint16_t>(name, static_cast<uint16_t>(param));
    if (_callback_manager.check_call_type<bool, int8_t>(name))
        return _callback_manager.call<bool, int8_t>(name, static_cast<int8_t>(param));
    return _callback_manager.call<bool, uint8_t>(name, static_cast<uint8_t>(param));
}

bool Configurable::set_conf_str(const std::string & name, const std::string & param)
{
    if (_callback_manager.check_call_type<bool, const std::string &>(name))
        return _callback_manager.call<bool, const std::string &>(name, param);
    if (_callback_manager.check_call_type<bool, std::string_view>(name))
        return _callback_manager.call<bool, std::string_view>(name, param);
    return _callback_manager.call<bool, const char *>(name, param.c_str());
}

bool Configurable::set_conf_str(const std::string & name, std::string_view param)
{
    if (_callback_manager.check_call_type<bool, std::string_view>(name))
        return _callback_manager.call<bool, std::string_view>(name, param);
    if (_callback_manager.check_call_type<bool, const char *>(name))
        return _callback_manager.call<bool, const char *>(name, param.data());
    std::string str(param.data(), param.size());
    return this->set_conf_str(name, str);
}

} // namespace sihd::util
