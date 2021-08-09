#ifndef __SIHD_UTIL_CALLBACK_HPP__
#define __SIHD_UTIL_CALLBACK_HPP__

#include <functional>
#include <map>
#include <string>

namespace sihd::util
{

class CallbackManager {
    public:
        CallbackManager() {}
        ~CallbackManager() {}

        // Non-member functions binding
        template<typename R, typename ... Targs>
        void set(std::string name, R (*func)(Targs ...)) {
            _callbacks[name] = new Callback<R, Targs ...>([func](Targs ... args) {
                return (*func)(args ...);
            });
        }

        // Member functions binding
        template<typename C, typename R, typename ... Targs>
        void set(std::string name, C* obj, R (C::*func)(Targs ...)) {
            _callbacks[name] = new Callback<R, Targs ...>([func, obj](Targs ... args) {
                return (obj->*func)(args ...);
            });
        }

        // std::function binding
        template<typename R, typename ... Targs>
        void set(std::string name, std::function<R (Targs ...)> func) {
            _callbacks[name] = new Callback<R, Targs ...>(func);
        }

        // The entire signature of the lambda must be passed to this overload.
        template<typename R, typename ... Targs, typename Callable>
        void set(std::string name, Callable func) {
            std::function<R (Targs ...)> f(func);
            _callbacks[name] = new Callback<R, Targs ...>(f);
        }

        // Calling
        template<typename ... Targs>
        void call(const std::string& name, Targs ... args) {
            this->call_base<void, Targs ...>(name, args ...);
        }

        template<typename R, typename ... Targs>
        R call(const std::string& name, Targs ... args) {
            return this->call_base<R, Targs ...>(name, args ...);
        }

    private:
        // Class stored in CallbackManager's map 
        class CallbackBase {
            public:
                CallbackBase() {}
                virtual ~CallbackBase() {}
        };

        template<typename R, typename ... Targs>
        class Callback : public CallbackBase {
            public:
                Callback(std::function<R (Targs ...)> func) {
                    _func = func;
                }

                ~Callback() {}

                R call(Targs ... args) {
                    return _func(args ...);
                }

            private:
                std::function<R (Targs ...)> _func;
        };

        template<typename R, typename ... Targs>
        R call_base(const std::string& name, Targs ... args) {
            CallbackBase* cbase = _callbacks[name];
            if (cbase == nullptr)
                throw std::out_of_range("No callback named '" + name + "'.");

            Callback<R, Targs ...>* cb = dynamic_cast<Callback<R, Targs ...> *>(cbase);
            if (cb == nullptr)
                throw std::invalid_argument("Cast error for callback '" + name + "', check signature.");

            return cb->call(args ...);
        }

        std::map<std::string, CallbackBase*> _callbacks;
};

}

#endif 