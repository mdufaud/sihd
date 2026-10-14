#include <utility>

#include <sihd/sys/keyboard.hpp>

namespace sihd::sys::keyboard
{

Modifiers operator|(Modifiers lhs, Modifiers rhs)
{
    return static_cast<Modifiers>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

Modifiers operator&(Modifiers lhs, Modifiers rhs)
{
    return static_cast<Modifiers>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

} // namespace sihd::sys::keyboard
