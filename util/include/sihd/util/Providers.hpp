#ifndef __SIHD_UTIL_PROVIDERS_HPP__
#define __SIHD_UTIL_PROVIDERS_HPP__

#include <deque>
#include <functional>
#include <list>
#include <set>
#include <vector>

#include <sihd/util/Array.hpp>
#include <sihd/util/IProvider.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

template <template <typename...> typename Container, typename Type>
class Provider: public sihd::util::IProvider<Type>
{
    public:
        Provider(Container<Type> *container_ptr = nullptr) { this->set_container(container_ptr); }
        ~Provider() = default;

        bool provide(Type *value)
        {
            std::lock_guard l(_mutex);
            if (_iterator != _container_ptr->end())
            {
                *value = *_iterator;
                ++_iterator;
                return true;
            }
            return false;
        }

        bool providing() const
        {
            std::lock_guard l(_mutex);
            return _iterator != _container_ptr->end();
        }

        void set_container(Container<Type> *iterator)
        {
            {
                std::lock_guard l(_mutex);
                _container_ptr = iterator;
            }
            this->reset_index();
        }

        void reset_index()
        {
            std::lock_guard l(_mutex);
            if (_container_ptr == nullptr)
                return;
            _iterator = _container_ptr->begin();
        }

        Container<Type> *container() const { return _container_ptr; }
        typename Container<Type>::iterator iterator() const { return _iterator; }

    private:
        mutable std::mutex _mutex;
        typename Container<Type>::iterator _iterator;
        Container<Type> *_container_ptr;
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

template <typename Type>
class FunctionProvider: public sihd::util::IProvider<Type>
{
    public:
        using ProviderMethod = std::function<bool(Type *)>;
        using StatusMethod = std::function<bool()>;

        FunctionProvider() = default;

        FunctionProvider(ProviderMethod provider) { this->set_provider_function(std::move(provider)); }

        ~FunctionProvider() = default;

        bool provide(Type *value) { return _provide_method(value); }

        bool providing() const { return _providing_method ? _providing_method() : true; }

        void set_provider_function(ProviderMethod && fun) { _provide_method = std::move(fun); }
        void set_providing_function(StatusMethod && fun) { _providing_method = std::move(fun); }

    private:
        ProviderMethod _provide_method;
        StatusMethod _providing_method;
};

} // namespace sihd::util

#endif
