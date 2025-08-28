#ifndef __SIHD_UTIL_HANDLER_HPP__
#define __SIHD_UTIL_HANDLER_HPP__

#include <functional>

#include <sihd/util/IHandler.hpp>

namespace sihd::util
{

template <typename... T>
class Handler: public IHandler<T...>
{
    public:
        Handler() = default;

        template <typename Function>
        Handler(Function && fun)
        {
            this->set_method(std::move(fun));
        }

        template <typename C>
        Handler(C *obj, void (C::*fun)(T...))
        {
            this->set_method(obj, fun);
        }

        Handler(IHandler<T...> *handler_ptr) { this->set_handler(handler_ptr); }

        ~Handler() = default;

        void set_handler(IHandler<T...> *handler_ptr)
        {
            _handle_fun = [handler_ptr](T... args) {
                handler_ptr->handle(args...);
            };
        }

        template <typename Function>
        void set_method(Function && fun)
        {
            _handle_fun = std::move(fun);
        }

        template <typename C>
        void set_method(C *obj, void (C::*fun)(T...))
        {
            _handle_fun = [obj, fun](T... args) {
                (obj->*fun)(args...);
            };
        }

        void handle(T... args)
        {
            if (_handle_fun)
                _handle_fun(args...);
        }

        void operator()(T... args) { this->handle(args...); };

        bool has_method() const { return _handle_fun ? true : false; }

    protected:

    private:
        std::function<void(T...)> _handle_fun;
};

} // namespace sihd::util

#endif