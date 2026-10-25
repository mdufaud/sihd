#ifndef __SIHD_LUA_CHECKED_HPP__
#define __SIHD_LUA_CHECKED_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <fmt/format.h>

#include <sihd/util/Error.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::lua
{

// leaves the error on the stack and returns -1: the caller raises it once its C++ locals are gone
int push_error(lua_State *state, const sihd::util::Error & error);

namespace checked_detail
{

template <typename T>
struct ResultValue
{
        static_assert(false, "checked<Function>: Function must return a sihd::util::Result");
};

template <typename T>
struct ResultValue<sihd::util::Result<T>>
{
        using Type = T;
};

template <typename Arg>
using Stored = std::decay_t<Arg>;

template <typename Arg>
bool read_argument(lua_State *state, int & index, std::optional<Stored<Arg>> & slot, std::string & failure)
{
    if constexpr (std::is_same_v<Stored<Arg>, lua_State *>)
    {
        slot = state;
        return true;
    }
    else
    {
        auto value = luabridge::Stack<Stored<Arg>>::get(state, index);
        if (!value)
        {
            failure = fmt::format("argument {}: {}", index, value.message());
            return false;
        }
        slot = std::move(*value);
        ++index;
        return true;
    }
}

template <auto Function, typename Signature = decltype(Function)>
struct Checked;

template <auto Function, typename Return, typename... Args>
struct Checked<Function, Return (*)(Args...)>
{
        using Value = typename ResultValue<Return>::Type;

        static int call_and_push(lua_State *state) { return call_and_push(state, std::index_sequence_for<Args...> {}); }

        template <size_t... I>
        static int call_and_push(lua_State *state, std::index_sequence<I...>)
        {
            std::tuple<std::optional<Stored<Args>>...> slots;
            [[maybe_unused]] int index = 1;
            std::string failure;
            if (!(read_argument<Args>(state, index, std::get<I>(slots), failure) && ...))
                return push_error(state, sihd::util::Error(sihd::util::ErrorCode::invalid_argument, failure));
            try
            {
                Return result = Function(std::move(*std::get<I>(slots))...);
                if (!result)
                    return push_error(state, result.error());
                if constexpr (std::is_void_v<Value>)
                    return 0;
                else
                {
                    auto pushed = luabridge::Stack<Value>::push(state, *result);
                    if (!pushed)
                        return push_error(state, sihd::util::Error(sihd::util::ErrorCode::unknown, pushed.message()));
                    return 1;
                }
            }
            catch (const std::invalid_argument & exception)
            {
                return push_error(state, sihd::util::Error(sihd::util::ErrorCode::invalid_argument, exception.what()));
            }
            catch (const std::exception & exception)
            {
                return push_error(state, sihd::util::Error(sihd::util::ErrorCode::unknown, exception.what()));
            }
        }

        // lua_error longjmps: nothing with a destructor may live in this frame
        static int raise(lua_State *state)
        {
            const int pushed = call_and_push(state);
            if (pushed >= 0)
                return pushed;
            return lua_error(state);
        }
};

} // namespace checked_detail

// a Result-returning function as a lua function: its value, or its Error raised to the pcall
template <auto Function>
inline constexpr lua_CFunction checked = &checked_detail::Checked<Function>::raise;

} // namespace sihd::lua

#endif
