#ifndef __SIHD_UTIL_SMARTNODEPTR_HPP__
#define __SIHD_UTIL_SMARTNODEPTR_HPP__

#include <memory>
#include <type_traits>

#include <sihd/util/Named.hpp>

namespace sihd::util
{

template <typename T>
struct SmartNodeDeleter
{
        static_assert(std::has_virtual_destructor_v<Named>,
                      "the deleter deletes through base pointers: Named must have a virtual destructor");
        static_assert(std::is_base_of_v<Named, T>, "SmartNodePtr manages Named-derived nodes only");

        void operator()(T *ptr)
        {
            if (ptr != nullptr && ptr->is_owned_by_parent() == false)
            {
                delete ptr;
            }
        }
};

template <class T>
using SmartNodePtr = std::unique_ptr<T, SmartNodeDeleter<T>>;

} // namespace sihd::util

#endif