#include <algorithm>

#include <sihd/util/Node.hpp>
#include <sihd/util/Logger.hpp>

#define MAX_LINK_RECURSION 20

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(Node);

SIHD_LOGGER;

Node::Node(const std::string & name, Node *parent):
    Named(name, parent)
{
}

Node::~Node()
{
    this->remove_children();
}

void    Node::add_child_unsafe(Named *child, bool ownership)
{
    if (this->add_child(child->name(), child, ownership) == false)
        throw Node::AlreadyHasChild(child->name());
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
    return this->add_child(child->name(), child, ownership);
}

bool    Node::add_child(const std::string & name, Named *child, bool ownership)
{
    if (this->get_child(name) != nullptr)
    {
        SIHD_LOG_WARN("Node: '{}' child '{}' already exists",
                    this->full_name(), name);
        return false;
    }
    bool do_add = this->on_add_child(name, child);
    if (do_add)
    {
        if (child->parent() == nullptr)
            child->set_parent(this);
        ChildEntry *entry = new ChildEntry();
        entry->name = name;
        entry->obj = child;
        entry->ownership = ownership;
        _children_map[name] = entry;
        _children_keys.push_back(name);
    }
    return do_add;
}

Node::ChildEntry  *Node::_get_child_entry(const Named *child) const
{
    for (const auto & pair: _children_map)
    {
        if (pair.second->obj == child)
            return pair.second;
    }
    return nullptr;
}

bool    Node::has_ownership(const Named *child) const
{
    ChildEntry *entry = this->_get_child_entry(child);
    return entry != nullptr && entry->ownership;
}

bool    Node::has_ownership(const std::string & name) const
{
    ChildEntry *entry = this->_get_child_entry(name);
    return entry != nullptr && entry->ownership;
}

bool    Node::set_child_ownership(const Named *child, bool ownership)
{
    ChildEntry *entry = this->_get_child_entry(child);
    if (entry != nullptr)
        entry->ownership = ownership;
    return entry != nullptr;
}

bool    Node::set_child_ownership(const std::string & name, bool ownership)
{
    ChildEntry *entry = this->_get_child_entry(name);
    if (entry != nullptr)
        entry->ownership = ownership;
    return entry != nullptr;
}

Node::ChildEntry  *Node::_get_child_entry(const std::string & name) const
{
    auto it = _children_map.find(name);
    if (it != _children_map.end())
        return it->second;
    return nullptr;
}

Named   *Node::get_child(const std::string & name) const
{
    Node::ChildEntry *entry = this->_get_child_entry(name);
    return entry != nullptr ? entry->obj : nullptr;
}

bool    Node::remove_child(const Named *child)
{
    return this->remove_child(child->name());
}

bool    Node::remove_child(const std::string & name)
{
    return this->_remove_child_entry(this->_get_child_entry(name));
}

bool    Node::_remove_child_entry(Node::ChildEntry *entry)
{
    if (entry == nullptr)
        return false;
    this->on_remove_child(entry->name, entry->obj);
    if (entry->ownership && entry->obj != nullptr)
    {
        delete entry->obj;
    }
    auto it = std::find(_children_keys.begin(), _children_keys.end(), entry->name);
    if (it != _children_keys.end())
        _children_keys.erase(it);
    _children_map.erase(entry->name);
    delete entry;
    return true;
}

void    Node::remove_children()
{
    for (auto it = _children_map.begin(); it != _children_map.end(); )
    {
        ChildEntry *entry = it->second;
        it = _children_map.erase(it);
        this->_remove_child_entry(entry);
    }
    _children_map.clear();
    _children_keys.clear();
}

std::pair<std::string, std::string>     Node::parent_path(const std::string & path)
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
    if (_link_map.try_emplace(link, path).second == false)
    {
        SIHD_LOG_WARN("Node: '{}' link '{}' already exists", this->full_name(), link);
        return false;
    }
    _link_keys.push_back(link);
    return true;
}

bool    Node::remove_link(const std::string & link)
{
    _link_keys.erase(std::find(_link_keys.begin(), _link_keys.end(), link));
    return _link_map.erase(link) > 0;
}

Named   *Node::resolve_link(const std::string & path, size_t recursion)
{
    Named *child = this->find(path);
    if (child != nullptr)
        return child;
    if (recursion == MAX_LINK_RECURSION)
        throw Node::MaximumLinkRecursion();
    const auto [parent_path, child_name] = this->parent_path(path);
    Named *named = this->find(parent_path);
    if (named != nullptr)
    {
        Node *parent = dynamic_cast<Node *>(named);
        if (parent != nullptr)
        {
            parent->resolve_links(recursion + 1);
            child = this->find(path);
        }
    }
    return child;
}

bool    Node::on_check_link(const std::string & name, Named *child)
{
    (void)name;
    (void)child;
    return true;
}

bool    Node::on_add_child(const std::string & name, Named *child)
{
    (void)name;
    (void)child;
    return true;
}

void    Node::on_remove_child(const std::string & name, Named *child)
{
    (void)name;
    (void)child;
}

bool    Node::resolve_links(size_t recursion)
{
    Named *child;

    bool ret = true;
    for (const auto & link: _link_keys)
    {
        const auto & path = _link_map.at(link);
        child = this->resolve_link(path, recursion);
        if (child == nullptr)
        {
            SIHD_LOG_ERROR("Node: '{}' could not resolve link '{}' => '{}'",
                        this->full_name(),
                        link,
                        path);
            return false;
        }
        if (this->on_check_link(link, child))
            this->add_child(link, child, false);
        else
            ret = false;
    }
    return ret;
}

const std::map<std::string, Node::ChildEntry *> &   Node::children() const
{
    return _children_map;
}

const std::vector<std::string> &    Node::children_keys() const
{
    return _children_keys;
}

// TREE

void    Node::_tree_child_desc(std::string & s,
                                    const TreeOpts & opts,
                                    const std::string & indent,
                                    const std::string & name,
                                    const Named *child) const
{
    s += fmt::format("{}{}", indent, name);
    Node *parent = child->parent();
    if (name != child->name())
        s += fmt::format("  => {}", child->full_name());
    else if (parent != this)
        s += fmt::format("  -> {}", child->full_name());

    s += fmt::format(": {}", child->class_name());
    this->_add_tree_desc(s, opts, child);
    s += "\n";
    const Node *node = dynamic_cast<const Node *>(child);
    if (node != nullptr)
        node->_tree_children(s, opts);
}

void    Node::_iterate_tree_children(std::string & s, TreeOpts & opts, const std::string & indent) const
{
    for (const std::string & name: _children_keys)
    {
        Named *child = this->get_child(name);
        if (child != nullptr)
            this->_tree_child_desc(s, opts, indent, name, child);
    }
}

void    Node::_tree_children(std::string & s, TreeOpts opts) const
{
    opts.current_recursion += 1;
    if (opts.max_recursion != 0 && opts.max_recursion == opts.current_recursion)
        return ;
    if (_children_map.size() == 0)
        return ;
    std::string indent(opts.indent, ' ');
    opts.indent += opts.indent_by_iter;
    this->_iterate_tree_children(s, opts, indent);
}

void    Node::_add_tree_desc(std::string & s, const TreeOpts & opts, const Named *child) const
{
    if (opts.description)
    {
        std::string desc = child->description();
        if (desc.empty() == false)
            s += fmt::format(" - {}", desc);
    }
}

std::string     Node::tree_str() const
{
    return this->tree_str({});
}

std::string     Node::tree_desc_str() const
{
    TreeOpts opts;
    opts.description = true;
    return this->tree_str(opts);
}

std::string     Node::tree_str(TreeOpts opts) const
{
    std::string indent(opts.indent, ' ');
    opts.indent += opts.indent_by_iter;
    opts.current_recursion = 0;

    std::string s = fmt::format("{}{}: {}", indent, this->name(), this->class_name());
    this->_add_tree_desc(s, opts, this);
    s += "\n";
    this->_tree_children(s, opts);
    return s;
}

Node::AlreadyHasChild::AlreadyHasChild(std::string_view name)
{
    this->ex = fmt::format("Child {} already exists", name);
}

}