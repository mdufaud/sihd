#ifndef __SIHD_UTIL_OBSERVERWAITER_HPP__
# define __SIHD_UTIL_OBSERVERWAITER_HPP__

# include <sihd/util/Observable.hpp>
# include <sihd/util/IHandler.hpp>
# include <sihd/util/Waitable.hpp>

namespace sihd::util
{

template <typename T>
class ObserverWaiter: public IHandler<T *>
{
    public:
        ObserverWaiter(Observable<T> *obs): _obs_ptr(obs)
        {
            notifications = 0;
            obs->add_observer(this);
        }
        virtual ~ObserverWaiter()
        {
            _obs_ptr->remove_observer(this);
        }

        void handle([[maybe_unused]] T *obj)
        {
            ++notifications;
            _waitable.notify(1);
        }

        void wait()
        {
            _waitable.infinite_wait();
        }

        bool wait_for(time_t nano, uint32_t notifications = 1)
        {
            // waitable returns true when timed out
            return _waitable.wait_loop(nano, notifications) == false;
        }

        time_t notifications;

    protected:
    
    private:
        Observable<T> *_obs_ptr;
        Waitable _waitable;
};

}

#endif