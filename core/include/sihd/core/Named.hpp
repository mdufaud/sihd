#include <string>
#include <iostream>

namespace sihd::core
{

class NamedContainer;

class Named
{
    public:
        Named(const std::string &name, NamedContainer *parent = nullptr);
        ~Named();

        const std::string & get_name() const;

    private:
        std::string     _name;
        NamedContainer  *_parent_ptr;

};

}