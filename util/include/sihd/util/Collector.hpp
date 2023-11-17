#ifndef __SIHD_UTIL_COLLECTOR_HPP__
#define __SIHD_UTIL_COLLECTOR_HPP__

#include <atomic>

#include <sihd/util/IProvider.hpp>
#include <sihd/util/IStoppableRunnable.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

template <typename T>
class Collector: public IStoppableRunnable,
                 public Observable<Collector<T>>
{
    public:
        Collector(IProvider<T> *provider = nullptr, Timestamp duration = std::chrono::milliseconds(1)):
            _provider_ptr(provider),
            _running(false),
            _wait_duration(duration)
        {
        }

        virtual ~Collector() { this->stop(); }

        bool run()
        {
            if (_running.exchange(true) == true)
                return false;
            std::lock_guard l(_mutex);
            while (_running && _provider_ptr->providing())
            {
                if (_provider_ptr->provide(&_data))
                    this->notify_observers(this);
                else
                    _waitable.wait_for(_wait_duration, [this] { return _running == false; });
            }
            return true;
        }

        bool stop()
        {
            auto l = _waitable.guard();
            _running = false;
            _waitable.notify();
            return true;
        }

        void wait_stop()
        {
            this->stop();
            std::lock_guard l(_mutex);
        }

        bool is_running() const { return _running; }

        bool can_collect() const { return _provider_ptr != nullptr && _provider_ptr->providing(); }

        bool collect()
        {
            if (_provider_ptr->providing() && _provider_ptr->provide(&_data))
            {
                this->notify_observers();
                return true;
            }
            return false;
        }

        T data() const { return _data; }
        IProvider<T> *provider() const { return _provider_ptr; }
        Timestamp timeout_duration() const { return _wait_duration; }

        void set_provider(IProvider<T> *ptr) { _provider_ptr = ptr; }
        void set_timeout(Timestamp duration) { _wait_duration = duration; }

    protected:

    private:
        T _data;
        IProvider<T> *_provider_ptr;

        std::atomic<bool> _running;
        Timestamp _wait_duration;

        std::mutex _mutex;
        Waitable _waitable;
};

} // namespace sihd::util

#endif