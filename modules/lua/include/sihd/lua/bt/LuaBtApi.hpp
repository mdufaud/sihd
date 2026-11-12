#ifndef __SIHD_LUA_LUABTAPI_HPP__
#define __SIHD_LUA_LUABTAPI_HPP__

// clang-format off
#include <sihd/lua/Vm.hpp>
// clang-format on

#include <sihd/bt/Device.hpp>
#include <sihd/bt/Scanner.hpp>
#include <sihd/bt/adapter.hpp>

template <>
struct luabridge::Stack<sihd::bt::AddressType>: luabridge::Enum<sihd::bt::AddressType,
                                                                sihd::bt::AddressType::public_address,
                                                                sihd::bt::AddressType::random,
                                                                sihd::bt::AddressType::unspecified>
{
};

template <>
struct luabridge::Stack<sihd::bt::Transport>
    : luabridge::
          Enum<sihd::bt::Transport, sihd::bt::Transport::any, sihd::bt::Transport::le, sihd::bt::Transport::classic>
{
};

template <>
struct luabridge::Stack<sihd::bt::Scanner::Event>: luabridge::Enum<sihd::bt::Scanner::Event,
                                                                   sihd::bt::Scanner::Event::found,
                                                                   sihd::bt::Scanner::Event::updated,
                                                                   sihd::bt::Scanner::Event::lost>
{
};

template <>
struct luabridge::Stack<sihd::bt::WriteType>
    : luabridge::Enum<sihd::bt::WriteType, sihd::bt::WriteType::with_response, sihd::bt::WriteType::without_response>
{
};

template <>
struct luabridge::Stack<sihd::bt::PairingRequest::Kind>
    : luabridge::Enum<sihd::bt::PairingRequest::Kind,
                      sihd::bt::PairingRequest::Kind::confirm,
                      sihd::bt::PairingRequest::Kind::confirm_passkey,
                      sihd::bt::PairingRequest::Kind::display_passkey,
                      sihd::bt::PairingRequest::Kind::enter_passkey,
                      sihd::bt::PairingRequest::Kind::display_pin,
                      sihd::bt::PairingRequest::Kind::enter_pin>
{
};

template <>
struct luabridge::Stack<sihd::bt::Device::Event>: luabridge::Enum<sihd::bt::Device::Event,
                                                                  sihd::bt::Device::Event::connected,
                                                                  sihd::bt::Device::Event::disconnected,
                                                                  sihd::bt::Device::Event::notification>
{
};

namespace sihd::lua
{

class LuaBtApi
{
    public:
        static void load_all(Vm & vm);

        static void load_base(Vm & vm);

    private:
        LuaBtApi() = delete;
        ~LuaBtApi() = delete;
};

} // namespace sihd::lua

#endif
