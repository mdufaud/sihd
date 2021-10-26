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
            _handler_ptr = nullptr;
            _handle_fun = std::move(fun);
        }
        Handler(IHandler<T...> *handler)
        {
            _handler_ptr = handler;
        }
        virtual ~Handler() {};

        void handle(T... args)
        {
            if (_handler_ptr != nullptr)
                _handler_ptr->handle(args...);
            else if (_handle_fun)
                _handle_fun(args...);
        }

    protected:
    
    private:
        std::function<void(T...)> _handle_fun;
        IHandler<T...> *_handler_ptr;
};

}

#endif 