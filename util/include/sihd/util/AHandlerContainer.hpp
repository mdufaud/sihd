#ifndef __SIHD_UTIL_AHANDLERCONTAINER_HPP__
# define __SIHD_UTIL_AHANDLERCONTAINER_HPP__

# include <sihd/util/IHandler.hpp>

namespace sihd::util
{

template <typename ...T>
class AHandlerContainer
{
    public:
        AHandlerContainer(IHandler<T...> *handler_ptr = nullptr): _handler_ptr(handler_ptr) {}
        virtual ~AHandlerContainer() {};

        void set_handler(IHandler<T...> *handler_ptr)
        {
            _handler_ptr = handler_ptr;
        }

        void call_handler(T... args)
        {
            if (_handler_ptr != nullptr)
                _handler_ptr->handle(args...);
        }

        bool has_handler() const { return _handler_ptr != nullptr; }

    private:
        IHandler<T> *_handler_ptr
};

}

#endif