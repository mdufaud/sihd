#include <sihd/util/Node.hpp>
#include <sihd/util/Logger.hpp>

#define MAX_LINK_RECURSION 20


namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(Node);

LOGGER;

Node::Node(const std::string & name, Node *parent):
    Named(name, parent)
{
}

Node::~Node()
{
    this->delete_children();
}

void    Node::add_child_unsafe(Named *child, bool ownership)
{
    if (this->add_child(child->get_name(), child, ownership) == false)
        throw Node::AlreadyHasChild(child->get_name());
}

bool    Node::add_child(std::unique_ptr<Named> & unique)
{
    Named *internal_ptr = unique.release();
    return this->add_child(internal_ptr, true);
}

bool    Node::add_child(std::shared_ptr<Named> & shared)
{
    Named *internal_ptr = shared.get();
    return this->add_child(internal_ptr, false);
}

bool    Node::add_child(Named *child, bool ownership)
{
    return this->add_child(child->get_name(), child, ownership);
}

bool    Node::add_child(const std::string & name, Named *child, bool ownership)
{
    if (this->get_child(name) != nullptr)
    {
        LOG_WARN("Node: '%s' child '%s' already exists",
                    this->get_full_name().c_str(), name.c_str());
        return false;
    }
    if (child->get_parent() == nullptr)
        child->set_parent(this);
    ChildEntry *entry = new ChildEntry();
    entry->obj = child;
    entry->ownership = ownership;
    _children_map[name] = entry;
    return true;
}

bool    Node::has_ownership(const std::string & name)
{
    ChildEntry *entry = this->get_child_entry(name);
    if (entry != nullptr)
        return entry->ownership;
    return entry != nullptr;
}

bool    Node::set_child_ownership(const std::string & name, bool ownership)
{
    ChildEntry *entry = this->get_child_entry(name);
    if (entry != nullptr)
        entry->ownership = ownership;
    return entry != nullptr;
}

Node::ChildEntry  *Node::get_child_entry(const std::string & name) const
{
    if (_children_map.find(name) != _children_map.end())
        return _children_map.at(name);
    return nullptr;
}

Named   *Node::get_child(const std::string & name) const
{
    Node::ChildEntry *entry = this->get_child_entry(name);
    return entry != nullptr ? entry->obj : nullptr;
}


bool    Node::delete_child(const Named *child)
{
    return this->delete_child(child->get_name());
}

bool    Node::delete_child(const std::string & name)
{
    ChildEntry *entry = this->get_child_entry(name);
    if (entry != nullptr)
        _children_map.erase(name);
    return this->delete_child_entry(entry);
}

bool    Node::delete_child_entry(Node::ChildEntry *entry)
{
    if (entry == nullptr)
        return false;
    if (entry->ownership && entry->obj != nullptr)
        delete entry->obj;
    delete entry;
    return true;
}

void    Node::delete_children()
{
    for (auto it = _children_map.begin(); it != _children_map.end(); )
    {
        ChildEntry *entry = it->second;
        it = _children_map.erase(it);
        this->delete_child_entry(entry);
    }
}

Node  *Node::to_node(Named *child)
{
    if (child == nullptr)
        return nullptr;
    return dynamic_cast<Node *>(child);
}

const Node  *Node::to_cnode(const Named *child)
{
    if (child == nullptr)
        return nullptr;
    return dynamic_cast<const Node *>(child);
}

std::pair<std::string, std::string>     Node::get_parent_path(const std::string & path)
{
    std::pair<std::string, std::string> ret;
    size_t idx = path.find_last_of(Named::separator);
    if (idx == std::string::npos)
    {
        ret.first = path;
        return ret;
    }
    ret.first = path.substr(0, idx);
    ret.second = path.substr(idx + 1, path.length());
    return ret;
}

bool    Node::is_link(const std::string & name) const
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

bool    Node::_check_link(const std::string & name, Named *child)
{
    (void)name;
    (void)child;
    return true;
}

bool    Node::resolve_links(size_t recursion)
{
    Named *child;

    bool ret = true;
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
        if (this->_check_link(link, child))
            this->add_child(link, child, false);
        else
            ret = false;
    }
    return ret;
}

const std::map<std::string, Node::ChildEntry *> &    Node::get_children() const
{
    return _children_map;
}

std::vector<std::string>    Node::get_children_keys() const
{
    std::vector<std::string> ret;
    ret.reserve(_children_map.size());
    for (const auto & [name, entry]: _children_map)
    {
        (void)entry;
        ret.push_back(name);
    }
    return ret;
}

// TREE

void    Node::print_tree() const
{
    std::cout << this->get_tree_str({}) << std::endl;
}

void    Node::print_tree_desc() const
{
    std::cout << this->get_tree_desc_str() << std::endl;
}

void    Node::print_tree(TreeOpts opts) const
{
    std::cout << this->get_tree_str(opts) << std::endl;
}

void    Node::_get_tree_child_desc(std::stringstream & ss,
                                    const TreeOpts & opts,
                                    const std::string & indent,
                                    const std::string & name,
                                    const Named *child) const
{
    ss << indent << name;
    Node *parent = child->get_parent();
    if (name != child->get_name())
        ss << " => " << child->get_full_name();
    else if (parent != this)
        ss << " -> " << child->get_full_name();

    ss << ": " << child->get_class_name();
    this->_add_tree_desc(ss, opts, child);
    ss << std::endl;
    const Node *node = dynamic_cast<const Node *>(child);
    if (node != nullptr)
        node->_get_tree_children(ss, opts);
}

void    Node::_iterate_tree_children(std::stringstream & ss, TreeOpts & opts, const std::string & indent) const
{
    for (const auto & [name, entry]: _children_map)
        this->_get_tree_child_desc(ss, opts, indent, name, entry->obj);
}

void    Node::_get_tree_children(std::stringstream & ss, TreeOpts opts) const
{
    opts.current_recursion += 1;
    if (opts.max_recursion != 0 && opts.max_recursion == opts.current_recursion)
        return ;
    if (_children_map.size() == 0)
        return ;
    std::string indent(opts.indent, ' ');
    opts.indent += opts.indent_by_iter;
    this->_iterate_tree_children(ss, opts, indent);
}

void    Node::_add_tree_desc(std::stringstream & ss, const TreeOpts & opts, const Named *child) const
{
    if (opts.description)
    {
        std::string desc = child->get_description();
        if (desc.empty() == false)
            ss << " - " << desc;
    }
}

std::string     Node::get_tree_str() const
{
    return this->get_tree_str({});
}

std::string     Node::get_tree_desc_str() const
{
    TreeOpts opts;
    opts.description = true;
    return this->get_tree_str(opts);
}

std::string     Node::get_tree_str(TreeOpts opts) const
{
    std::string indent(opts.indent, ' ');
    opts.indent += opts.indent_by_iter;
    opts.current_recursion = 0;
    std::stringstream ss;

    ss << indent << this->get_name() << ": " << this->get_class_name();
    this->_add_tree_desc(ss, opts, this);
    ss << std::endl;
    this->_get_tree_children(ss, opts);
    return ss.str();
}

}