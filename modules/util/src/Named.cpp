#include <sihd/util/Logger.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

SIHD_LOGGER;

char Named::separator = '.';

Named::Named(const std::string & named, Node *parent)
{
    _name = named;
    _parent_ptr = nullptr;
    if (parent)
        parent->add_child_unsafe(this);
}

Named::~Named() = default;

bool Named::set_parent_ownership(bool ownership)
{
    Node *parent = this->parent();
    if (parent != nullptr)
        return parent->set_child_ownership(this, ownership);
    return false;
}

bool Named::is_owned_by_parent() const
{
    const Node *parent = this->parent();
    return parent != nullptr && parent->has_ownership(this);
}

bool Named::set_parent(Node *parent)
{
    if (_parent_ptr == nullptr)
        _parent_ptr = parent;
    return _parent_ptr == parent;
}

const std::string & Named::name() const
{
    return _name;
}

std::string Named::full_name() const
{
    std::string ret = this->name();
    const Node *parent = this->parent();
    while (parent != nullptr)
    {
        ret = parent->name() + Named::separator + ret;
        parent = parent->parent();
    }
    return ret;
}

std::string Named::class_name() const
{
    return str::demangle(typeid(*this).name());
}

const Node *Named::_root() const
{
    const Node *obj = this->parent();

    if (obj == nullptr)
        return dynamic_cast<const Node *>(this);

    const Node *tmp;
    while (obj != nullptr)
    {
        tmp = obj->parent();
        if (tmp == nullptr)
            break;
        obj = tmp;
    }
    return obj;
}

const Named *Named::_find_from(const Named *from, const std::string & path)
{
    Splitter splitter(Named::separator);
    auto tokens = splitter.split(path);
    const Named *child = from;
    const Node *parent = nullptr;
    for (const std::string & name : tokens)
    {
        if (child != nullptr)
            parent = dynamic_cast<const Node *>(child);
        if (parent == nullptr)
            return nullptr;
        child = parent->get_child(name);
        if (child == nullptr)
            return nullptr;
    }
    return child;
}

const Named *Named::_find(const std::string & path) const
{
    const Named *current = nullptr;
    size_t i = 0;
    size_t len = path.size();
    while (i < len && path[i] == Named::separator)
    {
        if (current == nullptr)
            current = this;
        else
            current = current->parent();
        ++i;
    }
    if (i == 0)
    {
        if (path.size() > 0 && path[0] == '/')
        {
            std::string search_path = path.substr(1);
            current = this->_root();
            return Named::_find_from(current, search_path);
        }
        else
            current = this;
    }

    return Named::_find_from(current, path);
}

SIHD_REGISTER_FACTORY(Named);

} // namespace sihd::util
