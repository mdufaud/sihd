#ifndef __SIHD_UTIL_CONFIGURABLE_HPP__
#define __SIHD_UTIL_CONFIGURABLE_HPP__

#include <cstdint>
#include <expected>
#include <functional>

#include <sihd/json/fwd.hpp>
#include <sihd/util/Callback.hpp>
#include <sihd/util/Error.hpp>

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
            _callback_manager.set<C, bool, T>(name, dynamic_cast<C *>(this), method);
        };

        template <class C, typename T1, typename T2>
        void add_conf(const std::string & name, bool (C::*method)(T1, T2))
        {
            _callback_manager.set<C, bool, T1, T2>(name, dynamic_cast<C *>(this), method);
        };

        template <class C, typename T1, typename T2, typename T3>
        void add_conf(const std::string & name, bool (C::*method)(T1, T2, T3))
        {
            _callback_manager.set<C, bool, T1, T2, T3>(name, dynamic_cast<C *>(this), method);
        };

        template <typename T>
        void add_conf(const std::string & name, std::function<bool(T)> fun)
        {
            _callback_manager.set<bool, T>(name, fun);
        };

        template <typename T1, typename T2>
        void add_conf(const std::string & name, std::function<bool(T1, T2)> fun)
        {
            _callback_manager.set<bool, T1, T2>(name, fun);
        };

        template <typename T1, typename T2, typename T3>
        void add_conf(const std::string & name, std::function<bool(T1, T2, T3)> fun)
        {
            _callback_manager.set<bool, T1, T2, T3>(name, fun);
        };

        template <typename T>
        std::expected<void, Error> set_conf(const std::string & name)
        {
            return this->set_conf(name, T {});
        }

        template <typename... T>
        std::expected<void, Error> set_conf(const std::string & name, T... params)
        {
            std::expected<void, Error> res = this->_check_conf_handler<T...>(name);
            if (!res)
                return res;
            return this->_call_conf<T...>(name, std::forward<T>(params)...);
        }

        std::expected<void, Error> set_conf_float(const std::string & name, double param);

        std::expected<void, Error> set_conf_int(const std::string & name, int64_t param);

        std::expected<void, Error> set_conf_str(const std::string & name, const std::string & param);

        std::expected<void, Error> set_conf_str(const std::string & name, std::string_view param);

        std::expected<void, Error> set_conf_str(const std::string & name, const char *param);

        // Json-based configuration
        std::expected<void, Error> set_conf(const sihd::json::Json & json);
        std::expected<void, Error> set_conf(const std::string & key, const sihd::json::Json & val);

    private:
        CallbackManager _callback_manager;

        std::expected<void, Error> _set_conf_from_json(const std::string & name, const sihd::json::Json & val);
        std::expected<void, Error> _set_conf_json(const std::string & name, const sihd::json::Json & val);

        static std::unexpected<Error> _no_conf_key(const std::string & name);

        template <typename... T>
        std::expected<void, Error> _check_conf_handler(const std::string & name)
        {
            if (_callback_manager.exists(name) == false)
                return this->_no_conf_key(name);
            if (_callback_manager.check_call_type<bool, T...>(name) == false)
                return std::unexpected(Error(ErrorCode::not_supported, "conf '{}': no handler for this value", name));
            return {};
        }

        template <typename... T>
        std::expected<void, Error> _call_conf(const std::string & name, T... args)
        {
            if (_callback_manager.call<bool, T...>(name, std::forward<T>(args)...))
                return {};
            return std::unexpected(Error(ErrorCode::invalid_argument, "conf '{}': value refused", name));
        }
};

} // namespace sihd::util

#endif
