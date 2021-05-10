#include <sihd/core/Named.hpp>
#include <sihd/core/Node.hpp>
#include <sihd/core/String.hpp>

namespace sihd::core
{

Named::Named(const std::string & named, Node *parent)
{
    _name = named;
    _parent_ptr = nullptr;
    if (parent)
        parent->add_child(this);
}

Named::~Named()
{
}

bool    Named::add_parent(Node *parent)
{
    if (_parent_ptr != nullptr)
        return false;
    return parent->add_child(this);
}

bool    Named::set_parent(Node *parent)
{
    if (_parent_ptr == nullptr)
        _parent_ptr = parent;
    return _parent_ptr == parent;
}

const std::string & Named::get_name() const
{
    return _name;
}

std::string     Named::get_path_name() const
{
    std::string ret = this->get_name();
    Node *parent = this->get_parent();
    while (parent != nullptr)
    {
        ret = parent->get_name() + "." + ret;
        parent = parent->get_parent();
    }
    return ret;
}

Node  *Named::get_parent() const
{
    return _parent_ptr;
}

std::string     Named::get_class_name() const
{
    return sihd::core::str::demangle(typeid(*this).name());
}

Node   *Named::get_root()
{
    Node *tmp;
    Node *obj = Node::to_node(this);

    while (obj != nullptr)
    {
        tmp = obj->get_parent();
        if (tmp == nullptr)
            break ;
        obj = tmp;
    }
    return obj;
}

Named   *Named::find(Node *parent, const std::string & path)
{
    auto tokens = str::split(path, ".");
    Named *child = parent;
    for (const std::string & name : tokens)
    {
        if (child != nullptr)
        {
            parent = Node::to_node(child);
            if (parent == nullptr)
                return nullptr;
        }
        child = parent->get_child(name);
        if (child == nullptr)
            return nullptr;
    }
    return child;
}

Named   *Named::find(const std::string & path)
{
    Named *current = nullptr;

    int i = 0;
    while (path[i] == '.')
    {
        if (current == nullptr)
            current = this;
        else
            current = current->get_parent();
        ++i;
    }
    if (i == 0)
        current = this->get_root();
    return this->find(Node::to_node(current), path);
}

}