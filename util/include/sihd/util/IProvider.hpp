#ifndef __SIHD_UTIL_IPROVIDER_HPP__
# define __SIHD_UTIL_IPROVIDER_HPP__

# include <mutex>

namespace sihd::util
{

template <typename ...T>
class IProvider
{
    public:
        virtual ~IProvider() {};
        // provides a data
		virtual bool provide(T... args) = 0;
        // checks if a data can be provided
        virtual bool can_provide() const = 0;
        // checks if the provider is still providing
        virtual bool providing() const = 0;
        // infinite wait for a new data
        virtual void wait_new_provider_data() = 0;
        // timed wait for a new data
        virtual bool wait_for_provider_data(time_t nano_duration) = 0;
        // lock the provider to get a provided data
        virtual std::lock_guard<std::mutex> lock_guard_provider() = 0;
};

}

#endif