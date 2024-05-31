#ifndef __SIHD_UTIL_COLLECTOR_HPP__
#define __SIHD_UTIL_COLLECTOR_HPP__

#include <atomic>

#include <sihd/util/BlockingService.hpp>
#include <sihd/util/IProvider.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Waitable.hpp>

#include <fmt/format.h>

namespace sihd::util
{

template <typename T>
class Collector: public Observable<Collector<T>>,
                 public BlockingService
{
    public:
        Collector(IProvider<T> *provider = nullptr, Timestamp duration = std::chrono::milliseconds(1)):
            _provider_ptr(provider),
            _running(false),
            _wait_duration(duration)
        {
        }

        ~Collector()
        {
            if (this->is_running())
            {
                this->stop();
                this->service_wait_stop();
            }
        }

        bool collect()
        {
            if (_provider_ptr->providing() && _provider_ptr->provide(&_data))
            {
                this->notify_observers();
                return true;
            }
            return false;
        }

        void set_timeout(Timestamp duration) { _wait_duration = duration; }
        void set_provider(IProvider<T> *ptr) { _provider_ptr = ptr; }

        T data() const { return _data; }
        IProvider<T> *provider() const { return _provider_ptr; }
        Timestamp timeout_duration() const { return _wait_duration; }
        bool can_collect() const { return _provider_ptr != nullptr && _provider_ptr->providing(); }

    protected:
        bool on_start() override
        {
            _running = true;
            while (_running && _provider_ptr->providing())
            {
                if (_provider_ptr->provide(&_data))
                    this->notify_observers(this);
                else
                    _waitable.wait_for(_wait_duration, [this] { return _running == false; });
            }
            return true;
        }

        bool on_stop() override
        {
            {
                auto l = _waitable.guard();
                _running = false;
                _waitable.notify();
            }
            return true;
        }

    private:
        T _data;
        IProvider<T> *_provider_ptr;

        bool _running;
        Timestamp _wait_duration;

        std::mutex _run_mutex;
        Waitable _waitable;
};

} // namespace sihd::util

#endif