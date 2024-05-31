#ifndef __SIHD_UTIL_CONFIGURABLE_HPP__
#define __SIHD_UTIL_CONFIGURABLE_HPP__

#include <nlohmann/json_fwd.hpp>

#include <functional>

#include <sihd/util/Callback.hpp>

namespace sihd::util
{

class Configurable
{
    public:
        Configurable() = default;
        virtual ~Configurable() = default;

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

        template <typename... T>
        bool set_conf(const std::string & name, T... params)
        {
            return _callbackManager.call<bool, T...>(name, params...);
        }

        bool set_conf_float(const std::string & name, double param);

        bool set_conf_int(const std::string & name, int64_t param);

        bool set_conf_str(const std::string & name, const std::string & param);

        bool set_conf_str(const std::string & name, std::string_view param);

        bool set_conf_str(const std::string & name, const char *param);

        bool set_conf(const std::string & key, const nlohmann::json & val);

        bool set_conf(const nlohmann::json && json);

        bool set_conf(const nlohmann::json & json);

    private:
        CallbackManager _callbackManager;

        bool _set_conf_json(const std::string & name, const nlohmann::json & val);
};

} // namespace sihd::util

#endif
