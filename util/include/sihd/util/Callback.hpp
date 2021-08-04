#ifndef __SIHD_UTIL_CALLBACK_HPP__
# define __SIHD_UTIL_CALLBACK_HPP__

# include <functional>
# include <map>

namespace sihd::util
{

class ACallbackBase
{
    public:
        ACallbackBase() {};
        virtual ~ACallbackBase() {};
};

template <typename R, typename ... T>
class ACallback: public ACallbackBase
{
    public:
        ACallback() {};
        virtual ~ACallback() {};

        virtual R    call(T... arg) = 0;
};

/*
template <class C, typename R, typename ...T>
class CallbackObj: public ACallback<R, T...>
{
    public:
        CallbackObj(C *obj, R (C::*fun)(T...))
        {
            _obj_ptr = obj;
            _fun_ptr = fun;
        };

        virtual ~CallbackObj() {};

        virtual R    call(T... args)
        {
            return (_obj_ptr->*_fun_ptr)(args...);
        };

    private:
        R (C::*_fun_ptr)(T...);
        C *_obj_ptr;
};
*/

template <typename R, typename ... T>
class CallbackFun: public ACallback<R, T...>
{
    public:
        CallbackFun(std::function<R(T...)> fun)
        {
            _fun = fun;
        };

        virtual ~CallbackFun() {};

        virtual R    call(T... args)
        {
            return _fun(args...);
        };

    private:
        std::function<R(T...)>    _fun;
};

class CallbackManager
{
    public:
        CallbackManager() {};
        ~CallbackManager()
        {
            for (const auto & pair: _callbacks)
            {
                if (pair.second != nullptr)
                    delete pair.second;
            }
        };

        template <class C>
        void    set(const std::string & name, C *obj, void (C::*fun)())
        {
            _callbacks[name] = new CallbackFun<void>([obj, fun] () -> void
            {
                (obj->*fun)();
            });
        }

        template <class C, typename R, typename ...T>
        void    set(const std::string & name, C *obj, R (C::*fun)(T...))
        {
            _callbacks[name] = new CallbackFun<R, T...>([obj, fun] (T... args) -> R
            {
                return (obj->*fun)(args...);
            });
        }

        void    set(const std::string & name, std::function<void()> fun)
        {
            _callbacks[name] = new CallbackFun<void>(fun);
        } 

        template <typename R, typename ...T>
        void    set(const std::string & name, std::function<R(T...)> fun)
        {
            _callbacks[name] = new CallbackFun<R, T...>(fun);
        }

        void    del(const std::string & name)
        {
            ACallbackBase *cb = _callbacks[name];
            if (cb != nullptr)
                delete cb;
            _callbacks[name] = nullptr;
        };

        void    call(const std::string & name)
        {
            ACallbackBase *acb = _callbacks[name];
            if (acb == nullptr)
                throw std::out_of_range("No callback named: " + name);
            ACallback<void> *cb = dynamic_cast<ACallback<void> *>(acb);
            if (cb == nullptr)
                throw std::invalid_argument("Dynamic cast type error for callback: " + name);
            cb->call();
        }

        template <typename R, typename ...T>
        R    call(const std::string & name, T... args)
        {
            ACallbackBase *acb = _callbacks[name];
            if (acb == nullptr)
                throw std::out_of_range("No callback named: " + name);
            ACallback<R, T...> *cb = dynamic_cast<ACallback<R, T...> *>(acb);
            if (cb == nullptr)
                throw std::invalid_argument("Dynamic cast type error for callback: " + name);
            return cb->call(args...);
        }

    private:
        std::map<std::string, ACallbackBase *>    _callbacks;
};

}

#endif 