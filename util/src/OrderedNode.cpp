#include <sihd/util/OrderedNode.hpp>
#include <algorithm>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(OrderedNode);

LOGGER;

OrderedNode::OrderedNode(const std::string & name, Node *parent): Node(name, parent)
{
}

OrderedNode::~OrderedNode()
{
}

void    OrderedNode::_iterate_tree_children(std::stringstream & ss, TreeOpts & opts, const std::string & indent)
{
    for (const std::string & name: _children_order)
    {
        Named *child = this->get_child(name);
        if (child != nullptr)
            this->_get_tree_child_desc(ss, opts, indent, name, child);
    }
}

bool    OrderedNode::add_child(const std::string & name, Named *child, bool ownership)
{
    bool ret = Node::add_child(name, child, ownership);
    if (ret)
        _children_order.push_back(name);
    return ret;
}

bool    OrderedNode::delete_child(const std::string & name)
{
    bool ret = Node::delete_child(name);
    if (ret)
    {
        auto it = std::find(_children_order.begin(), _children_order.end(), name);
        if (it != _children_order.end())
            _children_order.erase(it);
    }
    return ret;
}

void    OrderedNode::delete_children()
{
    Node::delete_children();
    _children_order.clear();
}

std::vector<std::string>    OrderedNode::get_children_keys()
{
    return _children_order;
}

}