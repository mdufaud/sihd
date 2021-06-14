#include <sihd/util/Node.hpp>
#include <sihd/util/Logger.hpp>

#define MAX_LINK_RECURSION 20

namespace sihd::util
{

LOGGER;

Node::Node(const std::string & name, Node *parent):
    Named(name, parent)
{
}

Node::~Node()
{
    this->delete_children();
}

void    Node::add_child_unsafe(Named *child)
{
    if (this->add_child(child->get_name(), child) == false)
        throw Node::AlreadyHasChild(child->get_name());
}

bool    Node::add_child(Named *child)
{
    return this->add_child(child->get_name(), child);
}

bool    Node::add_child(const std::string & name, Named *child)
{
    if (this->get_child(name) != nullptr)
    {
        LOG_WARN("Node: '%s' child '%s' already exists",
                    this->get_full_name().c_str(), name.c_str());
        return false;
    }
    if (child->get_parent() == nullptr)
        child->set_parent(this);
    _children_map[name] = child;
    return true;
}

Named   *Node::get_child(const std::string & name)
{
    return _children_map[name];
}

bool    Node::delete_child(const Named *child)
{
    return this->delete_child(child->get_name());
}

bool    Node::delete_child(const std::string & name)
{
    Named *child = this->get_child(name);
    if (child == nullptr)
        return false;
    if (this->is_link(name) == false)
        delete child;
    _children_map.erase(name);
    return true;
}

void    Node::delete_children()
{
    for (auto & [name, child]: _children_map)
    {
        if (this->is_link(name) == false)
            delete child;
    }
    _children_map.clear();
}

Node  *Node::to_node(Named *child)
{
    if (child == nullptr)
        return nullptr;
    return dynamic_cast<Node *>(child);
}

std::pair<std::string, std::string>     Node::get_parent_path(const std::string & path)
{
    std::pair<std::string, std::string> ret;
    size_t idx = path.find_last_of('.');
    if (idx == std::string::npos)
    {
        ret.first = path;
        return ret;
    }
    ret.first = path.substr(0, idx);
    ret.second = path.substr(idx + 1, path.length());
    return ret;
}

bool    Node::is_link(const std::string & name)
{
    return _link_map.count(name) > 0;
}

bool    Node::add_link(const std::string & link, const std::string & path)
{
    if (_link_map.find(link) != _link_map.end())
    {
        LOG_WARN("Node: '%s' link '%s' already exists",
                    this->get_full_name().c_str(), link.c_str());
        return false;
    }
    _link_map[link] = path;
    return true;
}

bool    Node::remove_link(const std::string & link)
{
    return _link_map.erase(link) > 0;
}

Named   *Node::get_link(const std::string & path, size_t recursion)
{
    Named *child = this->find(path);
    if (child != nullptr)
        return child;
    if (recursion == MAX_LINK_RECURSION)
        throw Node::MaximumLinkRecursion();
    auto [parent_path, child_name] = this->get_parent_path(path);
    (void)child_name;
    Node *parent = this->to_node(this->find(parent_path));
    if (parent != nullptr)
    {
        parent->resolve_links(recursion + 1);
        child = this->find(path);
    }
    return child;
}

bool    Node::resolve_links(size_t recursion)
{
    Named *child;

    for (auto & [link, path]: _link_map)
    {
        child = this->get_link(path, recursion);
        if (child == nullptr)
        {
            LOG_ERROR("Node: '%s' could not resolve link '%s' => '%s'",
                        this->get_full_name().c_str(),
                        link.c_str(),
                        path.c_str());
            return false;
        }
        this->add_child(link, child);
    }
    return true;
}

void    Node::print_tree()
{
    std::cout << this->get_tree_str() << std::endl;
}

void    Node::_get_tree_children(std::stringstream & ss, TreeOpts opts)
{
    opts.current_recursion += 1;
    if (opts.max_recursion != 0 && opts.max_recursion == opts.current_recursion)
        return ;
    if (_children_map.size() == 0)
        return ;
    std::string indent(opts.indent, ' ');
    opts.indent += opts.indent_by_iter;
    for (const auto & [name, child]: _children_map)
    {
        ss << indent << name;
        Node *parent = child->get_parent();
        if (name != child->get_name())
            ss << " => " << child->get_full_name();
        else if (parent != this)
            ss << " -> " << child->get_full_name();

        ss << ": " << child->get_class_name() << std::endl;
        if (opts.description)
        {
            std::string desc = child->get_description();
            if (desc.empty() == false)
                ss << " - " << desc;
        }
        Node *node = this->to_node(child);
        if (node != nullptr)
            node->_get_tree_children(ss, opts);
    }
}

std::string     Node::get_tree_str()
{
    return this->get_tree_str({});
}

std::string     Node::get_tree_str(TreeOpts opts)
{
    std::string indent(opts.indent, ' ');
    opts.indent += opts.indent_by_iter;
    opts.current_recursion = 0;
    std::stringstream ss;

    ss << indent << this->get_name() << ": " << this->get_class_name() << std::endl;
    this->_get_tree_children(ss, opts);
    return ss.str();
}

}