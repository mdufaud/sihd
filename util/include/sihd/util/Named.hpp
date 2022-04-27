#ifndef __SIHD_UTIL_NAMED_HPP__
# define __SIHD_UTIL_NAMED_HPP__

# include <string>
# include <string_view>
# include <iostream>

namespace sihd::util
{

class Node;

class Named
{
    public:
        Named(const std::string & name, Node *parent = nullptr);
        virtual ~Named();

        const std::string & get_name() const;
        Node *get_parent() const;
        const Node *cget_parent() const;
        std::string get_full_name() const;
        std::string get_class_name() const;
        virtual std::string get_description() const
        {
            return "";
        };

        virtual bool set_parent(Node *parent);
        bool is_owned_by_parent() const;
        virtual bool set_parent_ownership(bool ownership);

        Node *get_root();
        const Node *cget_root() const;

        const Named *cfind(const Named *from, const std::string & path) const;
        const Named *cfind(const std::string & path) const;
        const Node *cfind_node(const std::string & path) const;
        template <class C>
        const C *cfind(const std::string & path) const
        {
            const Named *obj = this->cfind(path);
            if (obj != nullptr)
                return dynamic_cast<const C *>(obj);
            return nullptr;
        }

        Named *find(Named *from, const std::string & path);
        Named *find(const std::string & path);
        Node *find_node(const std::string & path);
        template <class C>
        C *find(const std::string & path)
        {
            Named *obj = this->find(path);
            if (obj != nullptr)
                return dynamic_cast<C *>(obj);
            return nullptr;
        }

        static char separator;

    protected:

    private:
        Node *_parent_ptr;
        std::string _name;

};

}

#endif