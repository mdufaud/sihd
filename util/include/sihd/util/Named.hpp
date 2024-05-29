#ifndef __SIHD_UTIL_NAMED_HPP__
#define __SIHD_UTIL_NAMED_HPP__

#include <array>
#include <string>
#include <string_view>

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

        template <class C>
        C *parent()
        {
            return dynamic_cast<C *>(_parent_ptr);
        }
        Node *parent();

        template <class C>
        const C *cparent() const
        {
            return dynamic_cast<const C *>(_parent_ptr);
        }
        const Node *cparent() const;

        Node *root();
        const Node *croot() const;

        static Named *find_from(Named *from, const std::string & path);
        static const Named *cfind_from(const Named *from, const std::string & path);

        Named *find(const std::string & path);
        template <class C>
        C *find(const std::string & path)
        {
            return dynamic_cast<C *>(this->find(path));
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

        template <class C>
        const C *cfind(const std::string & path) const
        {
            return dynamic_cast<const C *>(this->cfind(path));
        }
        const Named *cfind(const std::string & path) const;

        template <typename Type = Named, typename... Keys>
        std::array<const Type *, sizeof...(Keys)> cfind_all(const Keys &...args)
        {
            std::array<const Type *, sizeof...(Keys)> array;

            int i = 0;
            const auto finder = [&array, &i, this](const auto & arg) {
                array[i] = this->cfind<Type>(arg);
                ++i;
            };

            (finder(args), ...);

            return array;
        }

        static char separator;

    protected:

    private:
        Node *_parent_ptr;
        std::string _name;
};

} // namespace sihd::util

#endif