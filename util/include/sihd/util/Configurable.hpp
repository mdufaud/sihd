#ifndef __SIHD_UTIL_CONFIGURABLE_HPP__
# define __SIHD_UTIL_CONFIGURABLE_HPP__

# include <nlohmann/json.hpp>
# include <sihd/util/Callback.hpp>
# include <sihd/util/Logger.hpp>
# include <functional>

namespace sihd::util
{
SIHD_LOGGER;
class Configurable
{
    public:
        Configurable() {};
        virtual ~Configurable() {};

        template <class C, typename T>
        void add_conf(const std::string & name, bool (C::*method)(T))
        {
            _callbackManager.set<C, bool, T>(name, dynamic_cast<C *>(this), method);
        };

        template <class C, typename T1, typename T2>
        void add_conf(const std::string & name, bool (C::*method)(T1, T2))
        {
            _callbackManager.set<C, bool, T1, T2>(name, dynamic_cast<C *>(this), method);
        };

        template <class C, typename T1, typename T2, typename T3>
        void add_conf(const std::string & name, bool (C::*method)(T1, T2, T3))
        {
            _callbackManager.set<C, bool, T1, T2, T3>(name, dynamic_cast<C *>(this), method);
        };

        template <typename T>
        void add_conf(const std::string & name, std::function<bool(T)> fun)
        {
            _callbackManager.set<bool, T>(name, fun);
        };

        template <typename T1, typename T2>
        void add_conf(const std::string & name, std::function<bool(T1, T2)> fun)
        {
            _callbackManager.set<bool, T1, T2>(name, fun);
        };

        template <typename T1, typename T2, typename T3>
        void add_conf(const std::string & name, std::function<bool(T1, T2, T3)> fun)
        {
            _callbackManager.set<bool, T1, T2, T3>(name, fun);
        };

        template <typename T>
        bool set_conf(const std::string & name)
        {
            return _callbackManager.call<bool, T>(name);
        }

        template <typename ...T>
        bool set_conf(const std::string & name, T... params)
        {
            return _callbackManager.call<bool, T...>(name, params...);
        }

        bool set_conf_float(const std::string & name, double param)
        {
            try
            {
                return _callbackManager.call<bool, double>(name, param);
            }
            catch (const std::invalid_argument & e)
            {
                return _callbackManager.call<bool, float>(name, (float)param);
            }
        }

        bool set_conf_int(const std::string & name, int64_t param)
        {
            try { return _callbackManager.call<bool, int64_t>(name, param); }
            catch (const std::invalid_argument & e) {}
            try { return _callbackManager.call<bool, uint64_t>(name, param); }
            catch (const std::invalid_argument & e) {}
            try { return _callbackManager.call<bool, int32_t>(name, param); }
            catch (const std::invalid_argument & e) {}
            try { return _callbackManager.call<bool, uint32_t>(name, param); }
            catch (const std::invalid_argument & e) {}
            try { return _callbackManager.call<bool, int16_t>(name, param); }
            catch (const std::invalid_argument & e) {}
            try { return _callbackManager.call<bool, uint16_t>(name, param); }
            catch (const std::invalid_argument & e) {}
            try { return _callbackManager.call<bool, int8_t>(name, param); }
            catch (const std::invalid_argument & e) {}

            return _callbackManager.call<bool, uint8_t>(name, param);
        }

        bool set_conf_str(const std::string & name, const std::string & param)
        {
            try { return _callbackManager.call<bool, const std::string &>(name, param); }
            catch (const std::invalid_argument & e) {}
            try { return _callbackManager.call<bool, std::string_view>(name, param); }
            catch (const std::invalid_argument & e) {}
            return _callbackManager.call<bool, const char *>(name, param.c_str());
        }

        bool set_conf_str(const std::string & name, std::string_view param)
        {
            try { return _callbackManager.call<bool, std::string_view>(name, param); }
            catch (const std::invalid_argument & e) {}
            try { return _callbackManager.call<bool, const char *>(name, param.data()); }
            catch (const std::invalid_argument & e) {}
            std::string str(param.data(), param.size());
            return this->set_conf_str(name, str);
        }

        bool set_conf_str(const std::string & name, const char *param)
        {
            std::string_view str_view(param);
            return this->set_conf_str(name, str_view);
        }

        bool set_conf(const std::string & key, const nlohmann::json & val)
        {
            if (val.is_null())
                return false;
            if (val.is_object())
                return this->_set_conf_json(key, val);
            if (val.is_array())
            {
                bool ret = true;
                for (auto it = val.begin(); it != val.end(); ++it)
                {
                    if (this->set_conf(key, it.value()) == false)
                        ret = false;
                }
                return ret;
            }
            if (val.is_number_integer())
                return this->set_conf_int(key, val.get<int64_t>());
            if (val.is_number_float())
                return this->set_conf_float(key, val.get<double>());
            if (val.is_string())
                return this->set_conf_str(key, val.get<std::string>());
            if (val.is_boolean())
                return this->set_conf(key, val.get<bool>());
            return false;
        }

        bool set_conf(const nlohmann::json && json)
        {
            return this->set_conf(json);
        }

        bool set_conf(const nlohmann::json & json)
        {
            if (json.is_object() == false || json.is_null())
                return false;
            bool ret = true;
            for (auto it = json.begin(); it != json.end(); ++it)
            {
                if (this->set_conf(it.key(), it.value()) == false)
                    ret = false;
            }
            return ret;
        }

    private:
        CallbackManager _callbackManager;

        bool _set_conf_json(const std::string & name, const nlohmann::json & val)
        {
            try { return _callbackManager.call<bool, const nlohmann::json &>(name, val); }
            catch (const std::invalid_argument & e) {}
            return _callbackManager.call<bool, const nlohmann::json>(name, val);
        }
};

}

#endif