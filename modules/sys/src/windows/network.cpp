// clang-format off
#include <windows.h>
#include <netlistmgr.h>
// clang-format on

#include <optional>

#include <wrl/client.h>

#include <sihd/sys/network.hpp>
#include <sihd/sys/windows/ComInit.hpp>

#include "internal/com.hpp"

namespace sihd::sys::network
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using Microsoft::WRL::ComPtr;

Result<Connectivity> connectivity()
{
    std::optional<windows_::ComInit> com;
    auto initialized = internal::init_com(com);
    SIHD_UNEXPECTED_RETURN(initialized);

    ComPtr<INetworkListManager> manager;
    const HRESULT created = CoCreateInstance(CLSID_NetworkListManager,
                                             nullptr,
                                             CLSCTX_ALL,
                                             IID_PPV_ARGS(manager.GetAddressOf()));
    if (FAILED(created))
        return std::unexpected(
            Error(io_error, "could not reach the network list manager: 0x{:08x}", static_cast<ULONG>(created)));

    NLM_CONNECTIVITY flags = NLM_CONNECTIVITY_DISCONNECTED;
    const HRESULT read = manager->GetConnectivity(&flags);
    if (FAILED(read))
        return std::unexpected(
            Error(io_error, "could not read the network connectivity: 0x{:08x}", static_cast<ULONG>(read)));

    if (flags & (NLM_CONNECTIVITY_IPV4_INTERNET | NLM_CONNECTIVITY_IPV6_INTERNET))
        return Connectivity::full;
    if (flags
        & (NLM_CONNECTIVITY_IPV4_LOCALNETWORK | NLM_CONNECTIVITY_IPV6_LOCALNETWORK | NLM_CONNECTIVITY_IPV4_SUBNET
           | NLM_CONNECTIVITY_IPV6_SUBNET))
        return Connectivity::limited;
    return Connectivity::none;
}

} // namespace sihd::sys::network
