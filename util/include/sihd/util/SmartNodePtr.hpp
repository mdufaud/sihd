#ifndef __SIHD_UTIL_SMARTNODEPTR_HPP__
#define __SIHD_UTIL_SMARTNODEPTR_HPP__

#include <memory>

namespace sihd::util
{

template <typename T>
struct SmartNodeDeleter
{
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