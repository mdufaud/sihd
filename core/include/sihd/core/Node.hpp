#ifndef __SIHD_CORE_NODE_HPP__
# define __SIHD_CORE_NODE_HPP__

# include <sihd/core/Named.hpp>
# include <sihd/core/str.hpp>
# include <map>
# include <exception>

namespace sihd::core
{

class Node:   public Named
{
    public:

        class AlreadyHasChild:   public std::exception
        {
            public:
                AlreadyHasChild(std::string name)
                {
                    this->ex = str::format("Child %s already exists", name.c_str());
                };

                virtual const char *what() const throw()
                {
                    return ex.c_str();
                };

                std::string ex;
        };

        class MaximumLinkRecursion:   public std::exception
        {
            public:
                virtual const char *what() const throw()
                {
                    return "Maximum link recursion";
                }
        };

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
        void    add_child_unsafe(Named *child);
        bool    delete_child(const Named *child);
        bool    delete_child(const std::string & name);
        void    delete_children();

        // Static
        static Node    *to_node(Named *child);
        static std::pair<std::string, std::string>     get_parent_path(const std::string & path);

        // Find
        Node    *find_node(const std::string & path);
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
        Named   *get_link(const std::string & path, size_t recursion = 0);
        bool    resolve_links(size_t recursion = 0);

        std::string     get_tree_str();
        std::string     get_tree_str(TreeOpts opts);
        void            print_tree();

    protected:
        void    _get_tree_children(std::stringstream & ss, TreeOpts opts);

    private:
        std::map<std::string, Named *>   _children_map;
        std::map<std::string, std::string>   _link_map;
};

}

#endif