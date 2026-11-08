#ifndef __SIHD_UTIL_SMARTNODEPTR_HPP__
#define __SIHD_UTIL_SMARTNODEPTR_HPP__

#include <type_traits>

#include <sihd/util/Named.hpp>

namespace sihd::util
{

// a node owned by its parent when held is never read again: the parent may delete it first
class SmartNodeHolder
{
    public:
        explicit SmartNodeHolder(Named *node = nullptr);
        SmartNodeHolder(const SmartNodeHolder &) = delete;
        SmartNodeHolder & operator=(const SmartNodeHolder &) = delete;
        SmartNodeHolder(SmartNodeHolder && other) noexcept;
        SmartNodeHolder & operator=(SmartNodeHolder && other) noexcept;
        ~SmartNodeHolder();

        Named *get() const;

    private:
        void _delete();

        Named *_node;
        bool _deletes_node;
};

template <class T>
class SmartNodePtr
{
        static_assert(std::is_base_of_v<Named, T>, "SmartNodePtr manages Named-derived nodes only");

    public:
        SmartNodePtr() = default;
        explicit SmartNodePtr(T *ptr): _holder(ptr) {}

        T *get() const { return static_cast<T *>(_holder.get()); }
        T *operator->() const { return this->get(); }
        T & operator*() const { return *this->get(); }
        explicit operator bool() const { return _holder.get() != nullptr; }

    private:
        SmartNodeHolder _holder;
};

} // namespace sihd::util

#endif
