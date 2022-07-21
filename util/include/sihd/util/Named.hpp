#ifndef __SIHD_UTIL_NAMED_HPP__
# define __SIHD_UTIL_NAMED_HPP__

# include <string>
# include <string_view>

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
        virtual std::string description() const
        {
            return "";
        };

        Node *parent() const;
        const Node *cparent() const;

        template <class C>
        C *parent() const
        {
            return dynamic_cast<C *>(_parent_ptr);
        }
        template <class C>
        const C *cparent() const
        {
            return dynamic_cast<const C *>(_parent_ptr);
        }

        virtual bool set_parent(Node *parent);
        bool is_owned_by_parent() const;
        virtual bool set_parent_ownership(bool ownership);

        Node *root();
        const Node *croot() const;

        const Named *cfind(const Named *from, const std::string & path) const;
        const Named *cfind(const std::string & path) const;
        template <class C>
        const C *cfind(const std::string & path) const
        {
            return dynamic_cast<const C *>(this->cfind(path));
        }
        const Node *cfind_node(const std::string & path) const;

        Named *find(Named *from, const std::string & path);
        Named *find(const std::string & path);
        template <class C>
        C *find(const std::string & path)
        {
            return dynamic_cast<C *>(this->find(path));
        }
        Node *find_node(const std::string & path);

        static char separator;

    protected:

    private:
        Node *_parent_ptr;
        std::string _name;

};

}

#endif