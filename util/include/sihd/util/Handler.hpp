#ifndef __SIHD_UTIL_HANDLER_HPP__
# define __SIHD_UTIL_HANDLER_HPP__

# include <sihd/util/IHandler.hpp>
# include <functional>

namespace sihd::util
{

template <typename ...T>
class Handler: public IHandler<T...>
{
    public:
        Handler(std::function<void(T...)> fun)
        {
            _handle_fun = std::move(fun);
        }

        Handler()
        {
        }

        virtual ~Handler() {};

        void set_method(std::function<void(T...)> fun)
        {
            _handle_fun = std::move(fun);
        }

        template <typename C>
        void set_method(C* obj, void (C::*fun)(T...))
        {
            _handle_fun = [obj, fun] (T... args)
            {
                (obj->*fun)(args...);
            };
        }

        void handle(T... args)
        {
            if (_handle_fun)
                _handle_fun(args...);
        }

        bool has_method()
        {
            return _handle_fun ? true : false;
        }

    protected:
    
    private:
        std::function<void(T...)> _handle_fun;
};

}

#endif 