#ifndef __SIHD_UTIL_OBSERVABLE_HPP__
# define __SIHD_UTIL_OBSERVABLE_HPP__

# include <sihd/util/IObservable.hpp>
# include <sihd/util/IObserver.hpp>
# include <list>
# include <mutex>
# include <algorithm>

namespace sihd::util
{

template <typename T>
class Observable:   public IObservable<T>
{
    public:
        Observable() {};
        virtual ~Observable() {};

        bool add_observer(IObserver<T> *obs)
        {
            if (this->is_observer(obs))
                return false;
            std::lock_guard<std::mutex> l(_mutex);
            _observers.push_back(obs);
            return true;
        }

        bool remove_observer(IObserver<T> *obs)
        {
            if (this->is_observer(obs) == false)
                return false;
            std::lock_guard<std::mutex> l(_mutex_remove);
            _observers_to_remove.push_back(obs);
            return true;
        }

        bool is_observer(IObserver<T> *obs)
        {
            return std::find(_observers.begin(), _observers.end(), obs) != _observers.end();
        }

    protected:
        virtual void notify_observers(T *sender)
        {
            {
                std::lock_guard<std::mutex> rm_lock(_mutex_remove);
                for (const auto & obs: _observers_to_remove)
                {
                    _observers.remove(obs);
                }
                _observers_to_remove.clear();
            }
            std::lock_guard<std::mutex> lock(_mutex);
            for (const auto & obs: _observers)
            {
                obs->observable_changed(sender);
            }
        }

    private:
        std::mutex _mutex;
        std::mutex _mutex_remove;
        std::list<IObserver<T> *> _observers;
        std::list<IObserver<T> *> _observers_to_remove;
};

}

#endif 