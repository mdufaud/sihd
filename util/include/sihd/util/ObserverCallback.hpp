#ifndef __SIHD_UTIL_OBSERVERCALLBACK_HPP__
# define __SIHD_UTIL_OBSERVERCALLBACK_HPP__

# include <sihd/util/IObserver.hpp>
# include <functional>

namespace sihd::util
{

template <typename T>
class ObserverCallback: virtual public IObserver<T>
{
    public:
        ObserverCallback(std::function<void(T *)> fun)
        {
            callback = fun;
        };
        virtual ~ObserverCallback() {};

        virtual void    observable_changed(T *obs)
        {
            this->callback(obs);
        }

        std::function<void(T *)> callback;
};

}

#endif 