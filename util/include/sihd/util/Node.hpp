#ifndef __SIHD_UTIL_NODE_HPP__
# define __SIHD_UTIL_NODE_HPP__

# include <sihd/util/Named.hpp>
# include <sihd/util/NamedFactory.hpp>
# include <sihd/util/Str.hpp>
# include <map>
# include <exception>

namespace sihd::util
{

class Node:   public Named
{
    public:

        class AlreadyHasChild: public std::exception
        {
            public:
                AlreadyHasChild(std::string name)
                {
                    this->ex = Str::format("Child %s already exists", name.c_str());
                };

                virtual const char *what() const throw()
                {
                    return ex.c_str();
                };

                std::string ex;
        };

        class MaximumLinkRecursion: public std::exception
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

        struct  ChildEntry
        {
            Named *obj;
            bool ownership;
        };

        Node(const std::string & name, Node *parent = nullptr);
        virtual ~Node();

        // Children
        virtual bool add_child(const std::string & name, Named *child, bool ownership = true);
        virtual bool add_child(Named *child, bool ownership = true);
        
        template <typename T>
        T *add_child(const std::string & name)
        {
            T *child = new T(name);
            if (this->add_child(child, true) == false)
            {
                delete child;
                child = nullptr;
            }
            return child;
        }

        // Unsafe -> throws
        void add_child_unsafe(Named *child, bool ownership = true);

        bool delete_child_entry(ChildEntry *entry);
        bool delete_child(const Named *child);
        virtual bool delete_child(const std::string & name);
        virtual void delete_children();

        // Static
        static Node *to_node(Named *child);
        static std::pair<std::string, std::string> get_parent_path(const std::string & path);

        // Ownership
        bool has_ownership(const std::string & name);
        bool set_child_ownership(const std::string & name, bool ownership);

        // Find
        ChildEntry *get_child_entry(const std::string & name);
        Node *find_node(const std::string & path);
        Named *get_child(const std::string & name);
        template<class C>
        C *get_child(const std::string & name)
        {
            Named *obj = this->get_child(name);
            if (obj != nullptr)
                return dynamic_cast<C *>(obj);
            return nullptr;
        }

        // Links
        bool is_link(const std::string & link);
        bool add_link(const std::string & link, const std::string & path);
        bool remove_link(const std::string & link);
        Named *get_link(const std::string & path, size_t recursion = 0);
        bool resolve_links(size_t recursion = 0);

        std::string get_tree_str();
        std::string get_tree_desc_str();
        std::string get_tree_str(TreeOpts opts);
        void print_tree();
        void print_tree_desc();
        void print_tree(TreeOpts opts);

        std::map<std::string, ChildEntry *> & get_children();
        virtual std::vector<std::string> get_children_keys();
        
    protected:
        virtual bool _check_link(const std::string & name, Named *child);
        virtual void _get_tree_children(std::stringstream & ss, TreeOpts opts);
        virtual void _iterate_tree_children(std::stringstream & ss,
                                                TreeOpts & opts,
                                                const std::string & indent);
        virtual void _get_tree_child_desc(std::stringstream & ss,
                                                const TreeOpts & opts,
                                                const std::string & indent,
                                                const std::string & name,
                                                Named *child);
        virtual void _add_tree_desc(std::stringstream & ss, const TreeOpts & opts, Named *child);

    private:
        std::map<std::string, ChildEntry *> _children_map;
        std::map<std::string, std::string> _link_map;
};

}

#endif
