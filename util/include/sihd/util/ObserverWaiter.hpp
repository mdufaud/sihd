#ifndef __SIHD_UTIL_OBSERVERWAITER_HPP__
#define __SIHD_UTIL_OBSERVERWAITER_HPP__

#include <sihd/util/IHandler.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

template <typename T>
class ObserverWaiter: public IHandler<T *>
{
    public:
        ObserverWaiter(Observable<T> *obs = nullptr): _count(0), _obs_ptr(obs)
        {
            if (_obs_ptr != nullptr)
                _obs_ptr->add_observer(this);
        }
        virtual ~ObserverWaiter() { this->clear(); }

        void clear()
        {
            if (_obs_ptr != nullptr)
            {
                _obs_ptr->remove_observer(this);
                _obs_ptr = nullptr;
            }
            _waitable.notify_all();
            _count = 0;
        }

        bool observe(Observable<T> *obs)
        {
            if (obs == nullptr)
                return false;
            if (_obs_ptr == obs)
                return true;
            this->clear();
            if (obs->add_observer(this) == false)
                return false;
            _obs_ptr = obs;
            return true;
        }

        void handle([[maybe_unused]] T *obj)
        {
            ++_count;
            _waitable.notify(1);
        }

        void wait()
        {
            const uint32_t current = _count.load();
            _waitable.wait([this, current] { return _obs_ptr == nullptr || _count.load() != current; });
        }

        bool wait_for(sihd::util::Timestamp duration, uint32_t notifications = 1)
        {
            const uint32_t total = _count.load() + notifications;
            return _waitable.wait_for(duration,
                                      [this, total] { return _obs_ptr == nullptr || _count.load() >= total; });
        }

        uint32_t notifications() const { return _count.load(); }

    protected:

    private:
        std::atomic<uint32_t> _count;
        Waitable _waitable;
        Observable<T> *_obs_ptr;
};

} // namespace sihd::util

#endif