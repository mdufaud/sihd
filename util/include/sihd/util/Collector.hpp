#ifndef __SIHD_UTIL_COLLECTOR_HPP__
# define __SIHD_UTIL_COLLECTOR_HPP__

# include <sihd/util/IProvider.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Observable.hpp>

namespace sihd::util
{

template <typename T>
class Collector:    public IStoppableRunnable,
                    public Observable<Collector<T>>
{
    public:
        Collector(IProvider<T> *provider = nullptr, time_t milliseconds_timeout = 1):
            _provider_ptr(provider),
            _running(false),
            _provider_wait_milliseconds(milliseconds_timeout)
        {
        }

        virtual ~Collector()
        {
            this->stop();
        }

        bool run()
        {
            if (_running)
                return false;
            std::lock_guard l(_mutex);
            _running = true;
            while (_running)
            {
                // wait for a data
                while (_running && _provider_ptr->providing() && _provider_ptr->provider_empty())
                    _provider_ptr->provider_wait_for_data(_provider_wait_milliseconds);
                if (_running == false || _provider_ptr->providing() == false)
                    break ;
                // lock and get data
                {
                    _provider_ptr->provider_lock_guard();
                    if (_provider_ptr->provide(&_data))
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
            _running = false;
            std::lock_guard l(_mutex);
        }

        bool collect()
        {
            if (_provider_ptr->providing() && _provider_ptr->provider_empty() == false)
            {
                _provider_ptr->provider_lock_guard();
                if (_provider_ptr->provide(&_data))
                {
                    this->notify_observers();
                    return true;
                }
            }
            return false;
        }

        T & data() { return _data; }
        IProvider<T> *provider() const { return _provider_ptr; }
        time_t timeout_milliseconds() const { return _provider_wait_milliseconds; }

        void set_provider(IProvider<T> *ptr) { _provider_ptr = ptr; }
        void set_timeout_milliseconds(time_t milli) { _provider_wait_milliseconds = milli; }

    protected:
    
    private:
        T _data;
        IProvider<T> *_provider_ptr;
        bool _running;
        time_t _provider_wait_milliseconds;
        std::mutex _mutex;
};

}

#endif