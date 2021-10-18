#ifndef __SIHD_UTIL_PROVIDERS_HPP__
# define __SIHD_UTIL_PROVIDERS_HPP__

# include <vector>
# include <list>
# include <set>
# include <deque>
# include <functional>
# include <sihd/util/IProvider.hpp>
# include <sihd/util/Array.hpp>

# define __SIHD_ADD_ITERATOR_PROVIDER__(CLASSNAME, CONTAINER) \
template <typename TYPE> \
class CLASSNAME: public sihd::util::IProvider<TYPE &> \
{ \
    public: \
        CLASSNAME(CONTAINER<TYPE> *iterator = nullptr) { this->set_iterator(iterator); } \
        virtual ~CLASSNAME() {} \
 \
        bool provide(TYPE & value) \
        { \
            if (_iterator != _iterable_ptr->end()) \
            { \
                value = *_iterator; \
                ++_iterator; \
                return true; \
            } \
            return false; \
        } \
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
__SIHD_ADD_ITERATOR_PROVIDER__(VectorProvider, std::vector);
__SIHD_ADD_ITERATOR_PROVIDER__(ListProvider, std::list);
__SIHD_ADD_ITERATOR_PROVIDER__(SetProvider, std::set);
__SIHD_ADD_ITERATOR_PROVIDER__(DequeProvider, std::deque);
__SIHD_ADD_ITERATOR_PROVIDER__(ArrayProvider, sihd::util::Array);

template <typename ...TYPE>
class FunctionProvider: public sihd::util::IProvider<TYPE...>
{
    public:
        FunctionProvider() {}
        FunctionProvider(std::function<bool(TYPE...)> fun) { this->set_function(fun); }
        virtual ~FunctionProvider() {}

        bool provide(TYPE ...value)
        {
            return _method(value...);
        }

        void set_function(std::function<bool(TYPE...)> & fun) { _method = fun; }

    private:
        std::function<bool(TYPE...)> _method;
};


}

#endif