#ifndef __SIHD_UTIL_ORDEREDNODE_HPP__
# define __SIHD_UTIL_ORDEREDNODE_HPP__

# include <sihd/util/Node.hpp>
# include <list>

namespace sihd::util
{

class OrderedNode: public Node
{
    public:
        OrderedNode(const std::string & name, Node *parent = nullptr);
        virtual ~OrderedNode();

        using Node::add_child;
        using Node::delete_child;

        virtual bool    add_child(const std::string & name, Named *child, bool ownership = true) override;
        virtual bool    delete_child(const std::string & name) override;
        virtual void    delete_children() override;

        virtual std::vector<std::string>    get_children_keys() override;

    protected:
        virtual void    _iterate_tree_children(std::stringstream & ss, TreeOpts & opts, const std::string & indent) override;
    
    private:
        std::vector<std::string>  _children_order;
};

}

#endif 