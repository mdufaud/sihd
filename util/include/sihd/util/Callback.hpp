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

# define SIHD_MACRO_UNPACK(...) __VA_ARGS__

# define SIHD_MACRO_CB_MANAGER_SET(types, subtypes) \
template <typename R, types> \
void    set(const std::string & name, std::function<R(subtypes)> fun) \
{ \
    _callbacks[name] = new CallbackFun<R, subtypes>(fun); \
}

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
            _callbacks[name] = new CallbackObj<C, void>(obj, fun);
        }

        template <class C, typename R, typename ...T>
        void    set(const std::string & name, C *obj, R (C::*fun)(T...))
        {
            _callbacks[name] = new CallbackObj<C, R, T...>(obj, fun);
        }

        void    set(const std::string & name, std::function<void()> fun)
        {
            _callbacks[name] = new CallbackFun<void>(fun);
        } 

        template <typename R>
        void    set(const std::string & name, std::function<R()> fun)
        {
            _callbacks[name] = new CallbackFun<R>(fun);
        }
        
        SIHD_MACRO_CB_MANAGER_SET(SIHD_MACRO_UNPACK(typename T), SIHD_MACRO_UNPACK(T));
        SIHD_MACRO_CB_MANAGER_SET(SIHD_MACRO_UNPACK(typename T1, typename T2), SIHD_MACRO_UNPACK(T1, T2));
        SIHD_MACRO_CB_MANAGER_SET(SIHD_MACRO_UNPACK(typename T1, typename T2, typename T3), SIHD_MACRO_UNPACK(T1, T2, T3));

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
            return cb->call();
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