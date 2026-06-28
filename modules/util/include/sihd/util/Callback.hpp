#ifndef __SIHD_UTIL_CALLBACK_HPP__
#define __SIHD_UTIL_CALLBACK_HPP__

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace sihd::util
{

class CallbackManager
{
    public:
        CallbackManager() = default;

        ~CallbackManager() = default;

        bool exists(const std::string & name) { return _callbacks.find(name) != _callbacks.end(); }

        void remove(const std::string & name) { _callbacks.erase(name); }

        // Non-member functions binding
        template <typename R, typename... Targs>
        void set(const std::string & name, R (*fun)(Targs...))
        {
            _callbacks.emplace(name,
                               std::unique_ptr<CallbackBase>(new Callback<R, Targs...>(
                                   [fun](Targs... args) { return (*fun)(args...); })));
        }

        // Member functions binding
        template <typename C, typename R, typename... Targs>
        void set(const std::string & name, C *obj, R (C::*fun)(Targs...))
        {
            _callbacks.emplace(name,
                               std::unique_ptr<CallbackBase>(new Callback<R, Targs...>(
                                   [fun, obj](Targs... args) { return (obj->*fun)(args...); })));
        }

        // std::function binding
        template <typename R, typename... Targs>
        void set(const std::string & name, std::function<R(Targs...)> fun)
        {
            _callbacks.emplace(name,
                               std::unique_ptr<CallbackBase>(new Callback<R, Targs...>(std::move(fun))));
        }

        // The entire signature of the lambda must be passed to this overload.
        template <typename R, typename... Targs, typename Callable>
        void set(const std::string & name, Callable callable)
        {
            std::function<R(Targs...)> fun(callable);
            _callbacks.emplace(name,
                               std::unique_ptr<CallbackBase>(new Callback<R, Targs...>(std::move(fun))));
        }

        // Calling
        template <typename... Targs>
        void call(const std::string & name, Targs... args)
        {
            this->call_base<void, Targs...>(name, args...);
        }

        template <typename R, typename... Targs>
        R call(const std::string & name, Targs... args)
        {
            return this->call_base<R, Targs...>(name, args...);
        }

        template <typename R>
        R call(const std::string & name)
        {
            return this->call_base<R>(name);
        }

        template <typename R, typename... Targs>
        bool check_call_type(const std::string & name)
        {
            auto iter = _callbacks.find(name);
            if (iter == _callbacks.end() || !iter->second)
                return false;
            return dynamic_cast<Callback<R, Targs...> *>(iter->second.get()) != nullptr;
        }

    private:
        // Class stored in CallbackManager's map
        class CallbackBase
        {
            public:
                CallbackBase() = default;

                virtual ~CallbackBase() = default;
        };

        template <typename R, typename... Targs>
        class Callback: public CallbackBase
        {
            public:
                Callback(std::function<R(Targs...)> && fun) { _fun = std::move(fun); }

                ~Callback() = default;

                R call(Targs... args) { return _fun(args...); }

            private:
                std::function<R(Targs...)> _fun;
        };

        template <typename R, typename... Targs>
        R call_base(const std::string & name, Targs... args)
        {
            auto iter = _callbacks.find(name);
            if (iter == _callbacks.end() || !iter->second)
                throw std::out_of_range("No callback named '" + name + "'");

            Callback<R, Targs...> *cb = dynamic_cast<Callback<R, Targs...> *>(iter->second.get());
            if (cb == nullptr)
                throw std::invalid_argument("Cast error for callback '" + name + "', check signature");

            return cb->call(args...);
        }

        std::map<std::string, std::unique_ptr<CallbackBase>> _callbacks;
};

} // namespace sihd::util

#endif