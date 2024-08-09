#ifndef __SIHD_UTIL_OBSERVABLEDELEGATE_HPP__
#define __SIHD_UTIL_OBSERVABLEDELEGATE_HPP__

#include <sihd/util/Observable.hpp>

namespace sihd::util
{

template <typename T>
class ObservableDelegate: Observable<T>
{
    public:
        ObservableDelegate(T *observable_ptr): _observable_ptr(observable_ptr) {};
        ~ObservableDelegate() = default;

        void notify_observers_delegate() { this->notify_observers(_observable_ptr); }

        // Allow passing the observable to anyone, but prevent the usage of notify_observers
        class Protector
        {
            public:
                Protector(ObservableDelegate<T> & delegate): _delegate(delegate) {}
                ~Protector() = default;

                template <typename... Parameters>
                bool add_observer(Parameters &&...args)
                {
                    return _delegate.add_observer(std::forward<Parameters>(args)...);
                }

                template <typename... Parameters>
                bool remove_observer(Parameters &&...args)
                {
                    return _delegate.remove_observer(std::forward<Parameters>(args)...);
                }

                template <typename... Parameters>
                bool is_observer(Parameters &&...args)
                {
                    return _delegate.is_observer(std::forward<Parameters>(args)...);
                }

            private:
                ObservableDelegate<T> & _delegate;
        };

        Protector get_protected_obs() { return Protector(*this); };

    protected:

    private:
        T *_observable_ptr;
};

} // namespace sihd::util

#endif
