#ifndef __SIHD_UTIL_NODE_HPP__
#define __SIHD_UTIL_NODE_HPP__

#include <array>
#include <exception>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <sihd/util/Named.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/traits.hpp>

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

        sihd::util::Result<void> add_child(Named *child, bool take_ownership = true);
        virtual sihd::util::Result<void> add_child(const std::string & name, Named *child, bool take_ownership = true);

        template <typename T>
        T *add_child(const std::string & name)
        {
            T *child = new T(name);
            auto res = this->add_child(child, true);
            if (!res)
            {
                this->_log_add_child_error(res.error());
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

        bool has_ownership(const Named *child) const;
        bool has_ownership(const std::string & name) const;
        bool set_child_ownership(const std::string & name, bool ownership);
        bool set_child_ownership(const Named *child, bool ownership);

        template <class Self>
        auto get_child(this Self && self, const std::string & name) -> traits::const_prop_ptr<Self, Named>
        {
            const ChildEntry *entry = self._get_child_entry(name);
            if (entry == nullptr)
                return nullptr;
            return entry->obj;
        }

        template <class ChildType, class Self>
        auto get_child(this Self && self, const std::string & name) -> traits::const_prop_ptr<Self, ChildType>
        {
            using Ret = traits::const_prop_ptr<Self, ChildType>;
            return dynamic_cast<Ret>(self.get_child(name));
        }

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

        bool is_link(const std::string & link) const;
        bool add_link(const std::string & link, const std::string & path);
        bool remove_link(const std::string & link);
        Named *resolve_link(const std::string & path, size_t recursion = 0);
        sihd::util::Result<void> resolve_links(size_t recursion = 0);

        std::string tree_str() const { return this->tree_str({}); };
        std::string tree_str(TreeOpts opts) const;
        std::string tree_desc_str() const;

        const std::unordered_map<std::string, ChildEntry *> & children() const;
        const std::vector<std::string> & children_keys() const;

    protected:
        static std::pair<std::string, std::string> parent_path(const std::string & path);

        virtual bool on_check_link(const std::string & name, Named *child);
        virtual bool on_add_child(const std::string & name, Named *child);
        virtual void on_remove_child(const std::string & name, Named *child);

    private:
        sihd::util::Result<Named *> _resolve_link(const std::string & path, size_t recursion);
        void _log_add_child_error(const Error & error);
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
