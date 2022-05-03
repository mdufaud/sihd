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
            while (_running)
            {
                // wait for state change or data
                while (_running && _provider_ptr->providing() && _provider_ptr->provider_empty())
                    _provider_ptr->provider_wait_data_for(_provider_wait_milliseconds);
                // if either collector of provider stopped
                if (_running == false || _provider_ptr->providing() == false)
                    break ;
                // lock and get data if not empty
                {
                    _provider_ptr->provider_lock_guard();
                    if (_provider_ptr->provider_empty() == false && _provider_ptr->provide(&_data))
                        this->notify_observers(this);
                }
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
            return true;
        }

        void wait_stop()
        {
            this->stop();
            std::lock_guard l(_mutex);
        }

        bool collect()
        {
            if (_provider_ptr->providing())
            {
                _provider_ptr->provider_lock_guard();
                if (_provider_ptr->provider_empty() == false && _provider_ptr->provide(&_data))
                {
                    this->notify_observers();
                    return true;
                }
            }
            return false;
        }

        T & data() { return _data; }
        IProvider<T> *provider() const { return _provider_ptr; }
        time_t timeout_milliseconds() const { return sihd::util::time::to_ms(_provider_wait_milliseconds); }

        void set_provider(IProvider<T> *ptr) { _provider_ptr = ptr; }
        void set_timeout_milliseconds(time_t milli) { _provider_wait_milliseconds = sihd::util::time::ms(milli); }

    protected:

    private:
        T _data;
        IProvider<T> *_provider_ptr;
        std::atomic<bool> _running;
        time_t _provider_wait_milliseconds;
        std::mutex _mutex;
};

}

#endif