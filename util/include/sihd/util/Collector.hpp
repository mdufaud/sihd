#ifndef __SIHD_UTIL_COLLECTOR_HPP__
# define __SIHD_UTIL_COLLECTOR_HPP__

# include <sihd/util/IProvider.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Observable.hpp>

# include <atomic>

namespace sihd::util
{

template <typename T>
class Collector:    public IStoppableRunnable,
                    public Observable<Collector<T>>
{
    public:
        Collector(IProvider<T> *provider = nullptr, time_t milliseconds_timeout = 1):
            _provider_ptr(provider),
            _running(false)
        {
            this->set_timeout_milliseconds(milliseconds_timeout);
        }

        virtual ~Collector()
        {
            this->stop();
        }

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
                    _waitable.wait_for(_provider_nano_wait);
            }
            return true;
        }

        bool is_running() const
        {
            return _running;
        }

        bool stop()
        {
            _running = false;
            _waitable.notify(1);
            return true;
        }

        void wait_stop()
        {
            this->stop();
            std::lock_guard l(_mutex);
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

        T data() const { return _data; }
        IProvider<T> *provider() const { return _provider_ptr; }
        time_t timeout_milliseconds() const { return time::to_ms(_provider_nano_wait); }

        void set_provider(IProvider<T> *ptr) { _provider_ptr = ptr; }
        void set_timeout_milliseconds(time_t milli) { _provider_nano_wait = time::ms(milli); }

    protected:

    private:
        T _data;
        IProvider<T> *_provider_ptr;

        std::atomic<bool> _running;
        time_t _provider_nano_wait;

        std::mutex _mutex;
        Waitable _waitable;
};

}

#endif