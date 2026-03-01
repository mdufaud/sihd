#ifndef __SIHD_UTIL_NODE_HPP__
#define __SIHD_UTIL_NODE_HPP__

#include <exception>
#include <unordered_map>
#include <vector>

#include <sihd/util/Named.hpp>

namespace sihd::util
{

class Node: public Named
{
    public:
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

        Node(const std::string & name, Node *parent = nullptr);

        Node() = delete;
        Node(const Node & other) = delete;
        Node & operator=(const Node & other) = delete;
        Node & operator=(Node && other) = delete;

        virtual ~Node();

        // Children
        bool add_child(Named *child, bool take_ownership = true);
        virtual bool add_child(const std::string & name, Named *child, bool take_ownership = true);

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
        void add_child_unsafe(Named *child, bool take_ownership = true);

        bool remove_child(const Named *child);
        virtual bool remove_child(const std::string & name);

        virtual void remove_children();

        // Ownership
        bool has_ownership(const Named *child) const;
        bool has_ownership(const std::string & name) const;
        bool set_child_ownership(const std::string & name, bool ownership);
        bool set_child_ownership(const Named *child, bool ownership);

        template <class ChildType>
        ChildType *get_child(const std::string & name)
        {
            Named *obj = this->get_child(name);
            if (obj != nullptr)
                return dynamic_cast<ChildType *>(obj);
            return nullptr;
        }
        Named *get_child(const std::string & name);

        template <class ChildType = Named, typename... Keys>
        std::array<ChildType *, sizeof...(Keys)> get_all_child(const Keys &...args)
        {
            std::array<ChildType *, sizeof...(Keys)> array;

            int i = 0;
            const auto finder = [&array, &i, this](const auto & arg) {
                array[i] = this->get_child<ChildType>(arg);
                ++i;
            };

            (finder(args), ...);

            return array;
        }

        template <class ChildType>
        const ChildType *cget_child(const std::string & name) const
        {
            const Named *obj = this->cget_child(name);
            if (obj != nullptr)
                return dynamic_cast<const ChildType *>(obj);
            return nullptr;
        }
        const Named *cget_child(const std::string & name) const;

        template <class ChildType = Named, typename... Keys>
        std::array<const ChildType *, sizeof...(Keys)> cget_all_child(const Keys &...args) const
        {
            std::array<const ChildType *, sizeof...(Keys)> array;

            int i = 0;
            const auto finder = [&array, &i, this](const auto & arg) {
                array[i] = this->cget_child<ChildType>(arg);
                ++i;
            };

            (finder(args), ...);

            return array;
        }

        // preserves child order
        template <typename ChildType, typename Predicate>
        void execute_children(Predicate p)
        {
            for (const std::string & child_name : this->children_keys())
            {
                ChildType *child = dynamic_cast<ChildType *>(this->get_child(child_name));
                if (child != nullptr)
                    p(child);
            }
        }

        // Links
        bool is_link(const std::string & link) const;
        bool add_link(const std::string & link, const std::string & path);
        bool remove_link(const std::string & link);
        Named *resolve_link(const std::string & path, size_t recursion = 0);
        bool resolve_links(size_t recursion = 0);

        // Tree description
        std::string tree_str() const { return this->tree_str({}); };
        std::string tree_str(TreeOpts opts) const;
        std::string tree_desc_str() const;

        const std::unordered_map<std::string, ChildEntry *> & children() const;
        const std::vector<std::string> & children_keys() const;

    protected:
        // Static
        static std::pair<std::string, std::string> parent_path(const std::string & path);

        virtual bool on_check_link(const std::string & name, Named *child);
        virtual bool on_add_child(const std::string & name, Named *child);
        virtual void on_remove_child(const std::string & name, Named *child);

    private:
        bool _remove_child_entry(ChildEntry *entry);

        ChildEntry *_get_child_entry(const std::string & name) const;
        ChildEntry *_get_child_entry(const Named *child) const;

        std::vector<std::string> _children_keys;
        std::unordered_map<std::string, ChildEntry *> _children_map;
        std::vector<std::string> _link_keys;
        std::unordered_map<std::string, std::string> _link_map;
};

} // namespace sihd::util

#endif
