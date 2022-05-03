#ifndef __SIHD_UTIL_NODE_HPP__
# define __SIHD_UTIL_NODE_HPP__

# include <sihd/util/Named.hpp>
# include <sihd/util/NamedFactory.hpp>
# include <sihd/util/Str.hpp>
# include <map>
# include <exception>
# include <memory>

namespace sihd::util
{

class Node: public Named
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
                    return "Maximum node link recursion";
                }
        };

        struct TreeOpts
        {
            size_t indent = 0;
            size_t indent_by_iter = 2;
            size_t current_recursion = 0;
            size_t max_recursion = 0;
            bool description = false;
        };

        struct ChildEntry
        {
            std::string name;
            Named *obj;
            bool ownership;
        };

        Node() = delete;
        Node(const Node & other) = delete;
        Node & operator=(const Node & other) = delete;
        Node & operator=(Node && other) = delete;
        Node(const std::string & name, Node *parent = nullptr);
        virtual ~Node();

        // Children
        virtual bool add_child(const std::string & name, Named *child, bool take_ownership = true);
        virtual bool add_child(Named *child, bool take_ownership = true);

        // release internal pointer and takes ownership
        virtual bool add_child(std::unique_ptr<Named> & unique);
        // takes internal pointer and use it with no ownership - careful about pointer lifetimes
        virtual bool add_child(std::shared_ptr<Named> & shared);

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

        // unsafe -> throws
        virtual void add_child_unsafe(Named *child, bool take_ownership = true);

        virtual bool remove_child(const Named *child);
        virtual bool remove_child(const std::string & name);

        virtual bool delete_child(const Named *child);
        virtual bool delete_child(const std::string & name);

        virtual void delete_children();

        // Ownership
        bool has_ownership(const Named *child);
        bool has_ownership(const std::string & name);
        bool set_child_ownership(const std::string & name, bool ownership);
        bool set_child_ownership(const Named *child, bool ownership);

        // Find
        template <class C>
        C *get_child(const std::string & name) const
        {
            Named *obj = this->get_child(name);
            if (obj != nullptr)
                return dynamic_cast<C *>(obj);
            return nullptr;
        }
        Named *get_child(const std::string & name) const;
        ChildEntry *get_child_entry(const std::string & name) const;
        ChildEntry *get_child_entry(const Named *child) const;

        // Links
        bool is_link(const std::string & link) const;
        bool add_link(const std::string & link, const std::string & path);
        bool remove_link(const std::string & link);
        Named *get_link(const std::string & path, size_t recursion = 0);
        bool resolve_links(size_t recursion = 0);

        // Tree description
        std::string get_tree_str() const;
        std::string get_tree_desc_str() const;
        std::string get_tree_str(TreeOpts opts) const;
        void print_tree() const;
        void print_tree_desc() const;
        void print_tree(TreeOpts opts) const;

        // Static
        static Node *to_node(Named *child);
        static const Node *to_cnode(const Named *child);
        static std::pair<std::string, std::string> get_parent_path(const std::string & path);

        const std::map<std::string, ChildEntry *> & get_children() const;
        const std::vector<std::string> & get_children_keys() const;

    protected:
        virtual bool _remove_child_entry(ChildEntry *entry);
        virtual bool _delete_child_entry(ChildEntry *entry);

        virtual bool _check_link(const std::string & name, Named *child);

        virtual void _get_tree_children(std::stringstream & ss, TreeOpts opts) const;
        virtual void _iterate_tree_children(std::stringstream & ss,
                                                TreeOpts & opts,
                                                const std::string & indent) const;
        virtual void _get_tree_child_desc(std::stringstream & ss,
                                                const TreeOpts & opts,
                                                const std::string & indent,
                                                const std::string & name,
                                                const Named *child) const;
        virtual void _add_tree_desc(std::stringstream & ss, const TreeOpts & opts, const Named *child) const;

    private:
        std::vector<std::string> _children_keys;
        std::map<std::string, ChildEntry *> _children_map;
        std::map<std::string, std::string> _link_map;
};

}

#endif
