#ifndef __SIHD_UTIL_NAMED_HPP__
#define __SIHD_UTIL_NAMED_HPP__

#include <array>
#include <string>
#include <string_view>
#include <type_traits>

#include <sihd/util/traits.hpp>

namespace sihd::util
{

class Node;

class Named
{
    public:
        Named() = delete;
        Named(const Named & other) = delete;
        Named & operator=(const Named & other) = delete;
        Named & operator=(Named && other) = delete;

        Named(const std::string & name, Node *parent = nullptr);
        virtual ~Named();

        const std::string & name() const;
        std::string full_name() const;
        std::string class_name() const;
        virtual std::string description() const { return ""; };

        virtual bool set_parent(Node *parent);
        bool is_owned_by_parent() const;
        virtual bool set_parent_ownership(bool ownership);

        template <class Self>
        auto parent(this Self && self) -> traits::const_prop_ptr<Self, Node>
        {
            return self._parent_ptr;
        }

        template <class C, class Self>
        auto parent(this Self && self) -> traits::const_prop_ptr<Self, C>
        {
            using Ret = traits::const_prop_ptr<Self, C>;
            return dynamic_cast<Ret>(self.parent());
        }

        template <class Self>
        auto root(this Self && self) -> traits::const_prop_ptr<Self, Node>
        {
            constexpr bool is_const = std::is_const_v<std::remove_reference_t<Self>>;
            const Node *ret = self._root();
            if constexpr (is_const)
                return ret;
            else
                return const_cast<Node *>(ret);
        }

        template <class Self>
        auto find(this Self && self, const std::string & path) -> traits::const_prop_ptr<Self, Named>
        {
            constexpr bool is_const = std::is_const_v<std::remove_reference_t<Self>>;
            const Named *ret = self._find(path);
            if constexpr (is_const)
                return ret;
            else
                return const_cast<Named *>(ret);
        }

        template <class C, class Self>
        auto find(this Self && self, const std::string & path) -> traits::const_prop_ptr<Self, C>
        {
            using Ret = traits::const_prop_ptr<Self, C>;
            return dynamic_cast<Ret>(self.find(path));
        }

        template <typename Type = Named, typename... Keys>
        std::array<Type *, sizeof...(Keys)> find_all(const Keys &...args)
        {
            std::array<Type *, sizeof...(Keys)> array;

            int i = 0;
            const auto finder = [&array, &i, this](const auto & arg) {
                array[i] = this->find<Type>(arg);
                ++i;
            };

            (finder(args), ...);

            return array;
        }

        static char separator;

    private:
        const Node *_root() const;
        const Named *_find(const std::string & path) const;
        static const Named *_find_from(const Named *from, const std::string & path);

        Node *_parent_ptr;
        std::string _name;
};

} // namespace sihd::util

#endif
