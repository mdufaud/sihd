#ifndef __SIHD_UTIL_PROVIDERS_HPP__
# define __SIHD_UTIL_PROVIDERS_HPP__

# include <vector>
# include <list>
# include <set>
# include <deque>
# include <functional>

# include <sihd/util/IProvider.hpp>
# include <sihd/util/Array.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

template <template <typename...> typename CONTAINER, typename TYPE>
class Provider: public sihd::util::IProvider<TYPE>
{
    public:
        Provider(CONTAINER<TYPE> *container_ptr = nullptr) { this->set_container(container_ptr); }
        virtual ~Provider() {}

        bool provide(TYPE *value)
        {
            auto lock = std::lock_guard(_mutex);
            if (_iterator != _container_ptr->end())
            {
                *value = *_iterator;
                ++_iterator;
                return true;
            }
            return false;
        }

        virtual bool providing() const
        {
            auto lock = std::lock_guard(_mutex);
            return _iterator != _container_ptr->end();
        }

        void set_container(CONTAINER<TYPE> *iterator)
        {
            {
                auto lock = std::lock_guard(_mutex);
                _container_ptr = iterator;
                if (iterator == nullptr)
                    return ;
            }
            this->reset_index();
        }

        void reset_index()
        {
            auto lock = std::lock_guard(_mutex);
            _iterator = _container_ptr->begin();
        }

        CONTAINER<TYPE> *container() const { return _container_ptr; }
        typename CONTAINER<TYPE>::iterator iterator() const { return _iterator; }

    private:
        mutable std::mutex _mutex;
        typename CONTAINER<TYPE>::iterator _iterator;
        CONTAINER<TYPE> *_container_ptr;

};

template <typename T>
using VectorProvider = Provider<std::vector, T>;
template <typename T>
using ListProvider = Provider<std::list, T>;
template <typename T>
using SetProvider = Provider<std::set, T>;
template <typename T>
using DequeProvider = Provider<std::deque, T>;
template <typename T>
using ArrayProvider = Provider<sihd::util::Array, T>;

template <typename TYPE>
class FunctionProvider: public sihd::util::IProvider<TYPE>
{
    public:
        using ProviderMethod = std::function<bool (TYPE *)>;
        using StatusMethod = std::function<bool ()>;

        FunctionProvider() {}

        FunctionProvider(ProviderMethod provider)
        {
            this->set_provider_function(std::move(provider));
        }

        virtual ~FunctionProvider() {}

        bool provide(TYPE *value)
        {
            return _provide_method(value);
        }

        bool providing() const
        {
            return _providing_method ? _providing_method() : true;
        }

        void set_provider_function(ProviderMethod && fun) { _provide_method = std::move(fun); }
        void set_providing_function(StatusMethod && fun) { _providing_method = std::move(fun); }

    private:
        ProviderMethod _provide_method;
        StatusMethod _providing_method;
};


}

#endif