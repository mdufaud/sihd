#include <type_traits>
#include <utility>

#include <sihd/util/SmartNodePtr.hpp>

namespace sihd::util
{

static_assert(std::has_virtual_destructor_v<Named>,
              "the holder deletes through base pointers: Named must have a virtual destructor");

SmartNodeHolder::SmartNodeHolder(Named *node):
    _node(node),
    _deletes_node(node != nullptr && node->is_owned_by_parent() == false)
{
}

SmartNodeHolder::SmartNodeHolder(SmartNodeHolder && other) noexcept:
    _node(std::exchange(other._node, nullptr)),
    _deletes_node(std::exchange(other._deletes_node, false))
{
}

SmartNodeHolder & SmartNodeHolder::operator=(SmartNodeHolder && other) noexcept
{
    if (this != &other)
    {
        this->_delete();
        _node = std::exchange(other._node, nullptr);
        _deletes_node = std::exchange(other._deletes_node, false);
    }
    return *this;
}

SmartNodeHolder::~SmartNodeHolder()
{
    this->_delete();
}

Named *SmartNodeHolder::get() const
{
    return _node;
}

void SmartNodeHolder::_delete()
{
    if (_deletes_node)
        delete _node;
}

} // namespace sihd::util
