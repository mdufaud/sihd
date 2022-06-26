#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Str.hpp>
#include <sihd/util/Splitter.hpp>

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

Named::~Named()
{
}

bool    Named::set_parent_ownership(bool ownership)
{
    Node *parent = this->parent();
    if (parent != nullptr)
        return parent->set_child_ownership(this, ownership);
    return false;
}

bool    Named::is_owned_by_parent() const
{
    return this->parent() != nullptr
        ? this->parent()->has_ownership(this)
        : false;
}

bool    Named::set_parent(Node *parent)
{
    if (_parent_ptr == nullptr)
        _parent_ptr = parent;
    return _parent_ptr == parent;
}

const std::string & Named::name() const
{
    return _name;
}

std::string     Named::full_name() const
{
    std::string ret = this->name();
    Node *parent = this->parent();
    while (parent != nullptr)
    {
        ret = parent->name() + Named::separator + ret;
        parent = parent->parent();
    }
    return ret;
}

Node  *Named::parent() const
{
    return _parent_ptr;
}

const Node  *Named::cparent() const
{
    return _parent_ptr;
}

std::string     Named::class_name() const
{
    return sihd::util::Str::demangle(typeid(*this).name());
}

Node   *Named::root()
{
    return const_cast<Node *>(const_cast<Named *>(this)->croot());
}

const Node   *Named::croot() const
{
    const Node *obj = this->parent();

    if (obj == nullptr)
        return dynamic_cast<const Node *>(this);

    Node *tmp;
    while (obj != nullptr)
    {
        tmp = obj->parent();
        if (tmp == nullptr)
            break ;
        obj = tmp;
    }
    return obj;
}

Named   *Named::find(Named *from, const std::string & path)
{
    return const_cast<Named *>(const_cast<Named *>(this)->cfind(const_cast<Named *>(from), path));
}

Named   *Named::find(const std::string & path)
{
    return const_cast<Named *>(const_cast<Named *>(this)->cfind(path));
}

Node    *Named::find_node(const std::string & path)
{
    return const_cast<Node *>(const_cast<Named *>(this)->cfind_node(path));
}

const Named   *Named::cfind(const Named *from, const std::string & path) const
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

const Named   *Named::cfind(const std::string & path) const
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
            current = this->croot();
            return this->cfind(current, search_path);
        }
        else
            current = this;
    }
    return this->cfind(current, path);
}

const Node    *Named::cfind_node(const std::string & path) const
{
    return this->cfind<Node>(path);
}

}