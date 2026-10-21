#include <objbase.h>

#include <stdexcept>

#include <fmt/format.h>

#include <sihd/sys/os.hpp>
#include <sihd/sys/windows/ComInit.hpp>

namespace sihd::sys::windows_
{

ComInit::ComInit(Threading threading)
{
    const DWORD model = threading == Threading::apartment ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED;
    const HRESULT result = CoInitializeEx(nullptr, model | COINIT_DISABLE_OLE1DDE);
    // COM already runs on this thread under the other model: usable, not ours to uninitialize
    if (result == RPC_E_CHANGED_MODE)
        return;
    if (FAILED(result))
        throw std::runtime_error(fmt::format("could not CoInitializeEx: {}", os::error_str(static_cast<int>(result))));
    _initialized = true;
}

ComInit::~ComInit()
{
    if (_initialized)
        CoUninitialize();
}

} // namespace sihd::sys::windows_
