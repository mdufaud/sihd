#ifndef __SIHD_UTIL_DECORATOR_HPP__
#define __SIHD_UTIL_DECORATOR_HPP__

#include <sihd/util/Handler.hpp>
#include <sihd/util/Observable.hpp>

namespace sihd::util
{

template <typename T>
class Decorator
{
    public:
        Decorator(): _obs_ptr(nullptr) {}

        Decorator(Observable<T> *obs_ptr): Decorator()
        {
            if (this->decorate(obs_ptr) == false)
                throw std::runtime_error("Error while trying to decorate");
        }

        ~Decorator() { this->reset(); }

        bool decorate(Observable<T> *obs)
        {
            this->reset();
            constexpr bool add_to_front = true;
            if (obs->add_observer(&_handler_begin, add_to_front))
            {
                if (obs->add_observer(&_handler_end))
                    _obs_ptr = obs;
                else
                    obs->remove_observer(&_handler_begin);
            }
            return _obs_ptr == obs;
        }

        void reset()
        {
            if (_obs_ptr == nullptr)
                return;
            _obs_ptr->remove_observer(&_handler_begin);
            _obs_ptr->remove_observer(&_handler_end);
            _obs_ptr = nullptr;
        }

        void set_handler_begin(Handler<T *> handler) { _handler_begin = std::move(handler); }
        void set_handler_end(Handler<T *> handler) { _handler_end = std::move(handler); }

    private:
        Handler<T *> _handler_begin;
        Handler<T *> _handler_end;
        Observable<T> *_obs_ptr;
};

} // namespace sihd::util

#endif
