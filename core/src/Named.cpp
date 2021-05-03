#include <sihd/core/Named.hpp>

namespace sihd::core
{

Named::Named(const std::string & named, NamedContainer *parent)
{
    _name = named;
    _parent_ptr = parent;
}

Named::~Named()
{
}

const std::string &   Named::get_name() const
{
    return _name;
}

}