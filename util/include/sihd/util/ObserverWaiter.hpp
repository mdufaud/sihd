#ifndef __SIHD_UTIL_OBSERVERWAITER_HPP__
#define __SIHD_UTIL_OBSERVERWAITER_HPP__

#include <atomic>
#include <stdexcept>

#include <sihd/util/IHandler.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

template <typename T>
class ObserverWaiter: public IHandler<T *>
{
    public:
        ObserverWaiter(Observable<T> *obs = nullptr): _current_count(0), _last_waited_count(0), _obs_ptr(nullptr)
        {
            if (this->observe(obs) == false)
                throw std::runtime_error("Error while adding observer");
        }
        virtual ~ObserverWaiter() { this->clear(); }

        void clear()
        {
            auto l = _waitable.guard();
            if (_obs_ptr != nullptr)
            {
                _obs_ptr->remove_observer(this);
                _obs_ptr = nullptr;
            }
            _current_count = 0;
            _last_waited_count = 0;
            _waitable.notify_all();
        }

        bool observe(Observable<T> *obs)
        {
            if (obs == nullptr)
                return false;
            if (_obs_ptr != obs)
            {
                this->clear();
                if (obs->add_observer(this) == false)
                    return false;
                _obs_ptr = obs;
            }
            return true;
        }

        void handle([[maybe_unused]] T *obj)
        {
            auto l = _waitable.guard();
            ++_current_count;
            _waitable.notify(1);
        }

        // wait for a fixed number of notification
        void wait_nb(uint32_t total)
        {
            _waitable.wait([this, total] { return _obs_ptr == nullptr || _current_count.load() >= total; });
            _last_waited_count = _current_count.load();
        }

        // wait for a fixed number of notification
        bool wait_for_nb(sihd::util::Timestamp duration, uint32_t total)
        {
            const bool ret = _waitable.wait_for(duration, [this, total] {
                return _obs_ptr == nullptr || _current_count.load() >= total;
            });
            _last_waited_count = _current_count.load();
            return ret;
        }

        // wait x new notifications since this call
        void wait(uint32_t notifications = 1)
        {
            const uint32_t total = _current_count.load() + notifications;
            this->wait_nb(total);
        }

        // wait x new notifications since this call
        bool wait_for(sihd::util::Timestamp duration, uint32_t notifications = 1)
        {
            const uint32_t total = _current_count.load() + notifications;
            return this->wait_for_nb(duration, total);
        }

        // wait with previous waited number of notifications
        // use this if you write and wait in the same thread
        void prev_wait(uint32_t notifications = 1)
        {
            const uint32_t total = _current_count.load() + notifications;
            this->wait_nb(total);
        }

        // wait with previous waited number of notifications
        // use this if you write and wait in the same thread
        bool prev_wait_for(sihd::util::Timestamp duration, uint32_t notifications = 1)
        {
            const uint32_t total = _last_waited_count.load() + notifications;
            return this->wait_for_nb(duration, total);
        }

        Observable<T> *observing() const { return _obs_ptr; }

        uint32_t notifications() const { return _current_count.load(); }

    protected:

    private:
        std::atomic<uint32_t> _current_count;
        std::atomic<uint32_t> _last_waited_count;
        Waitable _waitable;
        Observable<T> *_obs_ptr;
};

} // namespace sihd::util

#endif