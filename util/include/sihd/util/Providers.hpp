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

# define __SIHD_ADD_ITERATOR_PROVIDER__(CLASSNAME, CONTAINER) \
template <typename TYPE> \
class CLASSNAME: public sihd::util::AProvider<TYPE> \
{ \
    public: \
        CLASSNAME(CONTAINER<TYPE> *iterator = nullptr) { this->set_iterator(iterator); } \
        virtual ~CLASSNAME() {} \
 \
        bool provide(TYPE *value) \
        { \
            if (_iterator != _iterable_ptr->end()) \
            { \
                *value = *_iterator; \
                ++_iterator; \
                return true; \
            } \
            return false; \
        } \
 \
        virtual bool providing() const  { return _iterator != _iterable_ptr->end(); } \
        virtual bool provider_empty() const  { return _iterator != _iterable_ptr->end(); } \
 \
        void set_iterator(CONTAINER<TYPE> *iterator) \
        { \
            _iterable_ptr = iterator; \
            if (iterator != nullptr) \
                this->reset_index(); \
        } \
 \
        void reset_index() { _iterator = _iterable_ptr->begin(); } \
 \
        CONTAINER<TYPE> *iterable() { return _iterable_ptr; } \
 \
    private: \
        typename CONTAINER<TYPE>::iterator _iterator; \
        CONTAINER<TYPE> *_iterable_ptr; \
};


namespace sihd::util
{

template <typename TYPE>
class AProvider: public sihd::util::IProvider<TYPE>
{
    public:
        virtual ~AProvider() {}

        virtual bool provider_wait_data_for(time_t nano_duration)
        {
            return _waitable.wait_for(nano_duration) == false;
        }

        virtual void provider_wait_data()
        {
            _waitable.infinite_wait();
        }

        std::lock_guard<std::mutex> provider_lock_guard()
        {
            return std::lock_guard(_mutex);
        }

    protected:
        void _provider_notify() { _waitable.notify(1); }
        void _provider_notify_all() { _waitable.notify_all(); }

    private:
        Waitable _waitable;
        std::mutex _mutex;
};

__SIHD_ADD_ITERATOR_PROVIDER__(VectorProvider, std::vector);
__SIHD_ADD_ITERATOR_PROVIDER__(ListProvider, std::list);
__SIHD_ADD_ITERATOR_PROVIDER__(SetProvider, std::set);
__SIHD_ADD_ITERATOR_PROVIDER__(DequeProvider, std::deque);
__SIHD_ADD_ITERATOR_PROVIDER__(ArrayProvider, sihd::util::Array);

template <typename TYPE>
class FunctionProvider: public sihd::util::AProvider<TYPE>
{
    public:
        FunctionProvider() {}

        FunctionProvider(std::function<bool(TYPE *)> provider)
        {
            this->set_provider_function(provider);
        }

        virtual ~FunctionProvider() {}

        bool provide(TYPE *value)
        {
            return _provide_method(value);
        }

        bool provider_empty() const
        {
            return _provider_empty_method ? _provider_empty_method() : true;
        }

        bool providing() const
        {
            return _providing_method ? _providing_method() : true;
        }

        void set_provider_function(std::function<bool(TYPE *)> & fun) { _provide_method = std::move(fun); }
        void set_provider_empty_function(std::function<bool()> & fun) { _provider_empty_method = std::move(fun); }
        void set_providing_function(std::function<bool()> & fun) { _providing_method = std::move(fun); }

    private:
        std::function<bool(TYPE *)> _provide_method;
        std::function<bool()> _provider_empty_method;
        std::function<bool()> _providing_method;
};


}

#endif