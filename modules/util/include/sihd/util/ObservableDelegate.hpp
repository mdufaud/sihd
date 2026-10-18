#ifndef __SIHD_UTIL_OBSERVABLEDELEGATE_HPP__
#define __SIHD_UTIL_OBSERVABLEDELEGATE_HPP__

#include <sihd/util/Observable.hpp>

namespace sihd::util
{

template <typename T>
class ObservableDelegate: public Observable<T>
{
    public:
        ObservableDelegate(T *observable_ptr): _observable_ptr(observable_ptr) {};
        ~ObservableDelegate() = default;

        void notify_observers_delegate() { this->notify_observers(_observable_ptr); }

    protected:

    private:
        T *_observable_ptr;
};

} // namespace sihd::util

#endif
