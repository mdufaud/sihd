#ifndef __SIHD_CORE_NODE_HPP__
# define __SIHD_CORE_NODE_HPP__

# include <sihd/core/Named.hpp>
# include <map>

namespace sihd::core
{

class Node:   public Named
{
    public:
        struct   TreeOpts
        {
            size_t  indent = 0;
            size_t  indent_by_iter = 2;
            size_t  current_recursion = 0;
            size_t  max_recursion = 0;
            bool    description = false;
        };

        Node(const std::string & name, Node *parent = nullptr);
        virtual ~Node();

        // Children
        bool    add_child(const std::string & name, Named *child);
        bool    add_child(Named *child);
        bool    delete_child(const Named *child);
        bool    delete_child(const std::string & name);
        void    delete_children();

        // Static
        static Node    *to_node(Named *child);
        static std::pair<std::string, std::string>     get_parent_path(const std::string & path);

        // Find
        Named   *get_child(const std::string & name);
        template<class C>
        C   *get_child(const std::string & name)
        {
            Named *obj = this->get_child(name);
            if (obj != nullptr)
                return dynamic_cast<C>(obj);
            return nullptr;
        }

        // Links
        bool    is_link(const std::string & link);
        bool    add_link(const std::string & link, const std::string & path);
        bool    remove_link(const std::string & link);
        Named   *get_link(const std::string & path);
        bool    resolve_links();

        std::string     get_tree_str();
        std::string     get_tree_str(TreeOpts opts);
        void            print_tree();

    protected:
        void    _get_tree_children(std::stringstream & ss, TreeOpts & opts);

    private:
        std::map<std::string, Named *>   _children_map;
        std::map<std::string, std::string>   _link_map;
};

}

#endif