#ifndef __SIHD_UTIL_SMARTNODEPTR_HPP__
# define __SIHD_UTIL_SMARTNODEPTR_HPP__

# include <sihd/util/Named.hpp>

namespace sihd::util
{

template <typename T>
struct SmartNodeDeleter
{
    void operator()(T* ptr)
    {
        if (ptr != nullptr)
        {
            Named *n = dynamic_cast<Named *>(ptr);
            if (n != nullptr)
            {
                if (n->is_owned_by_parent() == false)
                    delete n;
            }
            else
                delete ptr;
        }
    }
};

template <class T>
using SmartNodePtr = std::unique_ptr<T, SmartNodeDeleter<T>>;

}

#endif 