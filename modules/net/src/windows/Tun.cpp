#include <windows.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include <sihd/net/Tun.hpp>
#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/str.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::net
{

namespace
{

// a power of two between WINTUN_MIN_RING_CAPACITY and WINTUN_MAX_RING_CAPACITY
constexpr DWORD ring_capacity = 0x400000;
// WINTUN_MAX_IP_PACKET_SIZE
constexpr size_t max_packet_size = 0xffff;

using CreateAdapter = void *WINAPI(LPCWSTR, LPCWSTR, const GUID *);
using CloseAdapter = void WINAPI(void *);
using StartSession = void *WINAPI(void *, DWORD);
using EndSession = void WINAPI(void *);
using ReceivePacket = BYTE *WINAPI(void *, DWORD *);
using ReleaseReceivePacket = void WINAPI(void *, const BYTE *);
using AllocateSendPacket = BYTE *WINAPI(void *, DWORD);
using SendPacket = void WINAPI(void *, const BYTE *);
using GetReadWaitEvent = HANDLE WINAPI(void *);

Error wintun_error(std::string_view action, int code)
{
    return Error(code == ERROR_ACCESS_DENIED ? permission_denied : io_error,
                 "{}: {}",
                 action,
                 sihd::sys::os::error_str(code));
}

} // namespace

struct Tun::Impl
{
        sihd::sys::DynLib wintun;
        CloseAdapter *close_adapter = nullptr;
        EndSession *end_session = nullptr;
        ReceivePacket *receive = nullptr;
        ReleaseReceivePacket *release = nullptr;
        AllocateSendPacket *allocate = nullptr;
        SendPacket *send = nullptr;
        void *adapter = nullptr;
        void *session = nullptr;
        HANDLE read_event = nullptr;
};

Tun::Tun(): _impl(std::make_unique<Impl>()) {}

Tun::Tun(Tun && other): _impl(std::make_unique<Impl>())
{
    std::swap(_impl, other._impl);
}

Tun & Tun::operator=(Tun && other)
{
    if (this != &other)
    {
        this->close();
        std::swap(_impl, other._impl);
    }
    return *this;
}

Tun::~Tun()
{
    this->close();
}

Result<Tun> Tun::adopt([[maybe_unused]] int fd)
{
    return std::unexpected(Error(not_supported, "a windows tun is opened by name"));
}

Result<void> Tun::open(const std::string & name)
{
    if (name.empty() || name.size() > max_name_size)
        return std::unexpected(Error(invalid_argument, "tun name must hold 1 to {} characters", max_name_size));

    this->close();
    Impl & impl = *_impl;
    auto loaded = impl.wintun.open("wintun.dll");
    SIHD_UNEXPECTED_RETURN_CTX(loaded, "loading wintun.dll");

    auto create = impl.wintun.load_function<CreateAdapter>("WintunCreateAdapter");
    SIHD_UNEXPECTED_RETURN(create);
    auto start = impl.wintun.load_function<StartSession>("WintunStartSession");
    SIHD_UNEXPECTED_RETURN(start);
    auto close_adapter = impl.wintun.load_function<CloseAdapter>("WintunCloseAdapter");
    SIHD_UNEXPECTED_RETURN(close_adapter);
    auto end_session = impl.wintun.load_function<EndSession>("WintunEndSession");
    SIHD_UNEXPECTED_RETURN(end_session);

    auto receive = impl.wintun.load_function<ReceivePacket>("WintunReceivePacket");
    SIHD_UNEXPECTED_RETURN(receive);
    auto release = impl.wintun.load_function<ReleaseReceivePacket>("WintunReleaseReceivePacket");
    SIHD_UNEXPECTED_RETURN(release);
    auto allocate = impl.wintun.load_function<AllocateSendPacket>("WintunAllocateSendPacket");
    SIHD_UNEXPECTED_RETURN(allocate);
    auto send = impl.wintun.load_function<SendPacket>("WintunSendPacket");
    SIHD_UNEXPECTED_RETURN(send);
    auto read_wait_event = impl.wintun.load_function<GetReadWaitEvent>("WintunGetReadWaitEvent");
    SIHD_UNEXPECTED_RETURN(read_wait_event);

    impl.close_adapter = *close_adapter;
    impl.end_session = *end_session;
    impl.receive = *receive;
    impl.release = *release;
    impl.allocate = *allocate;
    impl.send = *send;

    // the adapter name doubles as its tunnel type, shown by windows
    const std::wstring wide_name = str::to_wstr(name);
    impl.adapter = (*create)(wide_name.c_str(), wide_name.c_str(), nullptr);
    if (impl.adapter == nullptr)
    {
        const Error error = wintun_error("creating wintun adapter", sihd::sys::os::last_error());
        this->close();
        return std::unexpected(error);
    }
    impl.session = (*start)(impl.adapter, ring_capacity);
    if (impl.session == nullptr)
    {
        const Error error = wintun_error("starting wintun session", sihd::sys::os::last_error());
        this->close();
        return std::unexpected(error);
    }
    impl.read_event = (*read_wait_event)(impl.session);
    return {};
}

void Tun::close()
{
    Impl & impl = *_impl;
    if (impl.session != nullptr)
        impl.end_session(impl.session);
    if (impl.adapter != nullptr)
        impl.close_adapter(impl.adapter);
    impl.session = nullptr;
    impl.adapter = nullptr;
    impl.read_event = nullptr;
    (void)impl.wintun.close();
}

Result<size_t> Tun::read(void *data, size_t size)
{
    Impl & impl = *_impl;
    if (impl.session == nullptr)
        return std::unexpected(Error(not_initialized, "tun is not open"));

    DWORD packet_size = 0;
    BYTE *packet = impl.receive(impl.session, &packet_size);
    if (packet == nullptr)
    {
        const int code = sihd::sys::os::last_error();
        if (code == ERROR_NO_MORE_ITEMS)
            return std::unexpected(Error(would_block, "no packet pending"));
        if (code == ERROR_HANDLE_EOF)
            return std::unexpected(Error(closed, "wintun adapter is terminating"));
        return std::unexpected(wintun_error("tun read", code));
    }
    const size_t copied = std::min<size_t>(packet_size, size);
    std::memcpy(data, packet, copied);
    impl.release(impl.session, packet);
    return copied;
}

Result<size_t> Tun::write(ArrUByteView packet)
{
    Impl & impl = *_impl;
    if (impl.session == nullptr)
        return std::unexpected(Error(not_initialized, "tun is not open"));
    if (packet.size() > max_packet_size)
        return std::unexpected(Error(invalid_argument, "a tun packet holds at most {} bytes", max_packet_size));

    BYTE *buffer = impl.allocate(impl.session, static_cast<DWORD>(packet.size()));
    if (buffer == nullptr)
    {
        const int code = sihd::sys::os::last_error();
        if (code == ERROR_BUFFER_OVERFLOW)
            return std::unexpected(Error(would_block, "wintun ring is full"));
        if (code == ERROR_HANDLE_EOF)
            return std::unexpected(Error(closed, "wintun adapter is terminating"));
        return std::unexpected(wintun_error("tun write", code));
    }
    std::memcpy(buffer, packet.data(), packet.size());
    impl.send(impl.session, buffer);
    return packet.size();
}

Result<bool> Tun::wait_readable(Duration timeout) const
{
    const Impl & impl = *_impl;
    if (impl.session == nullptr)
        return std::unexpected(Error(not_initialized, "tun is not open"));
    const DWORD waited = WaitForSingleObject(impl.read_event, static_cast<DWORD>(timeout.milliseconds_int()));
    if (waited == WAIT_FAILED)
        return std::unexpected(wintun_error("tun wait", sihd::sys::os::last_error()));
    return waited == WAIT_OBJECT_0;
}

bool Tun::is_open() const
{
    return _impl->session != nullptr;
}

Result<int> Tun::fd() const
{
    return std::unexpected(Error(not_supported, "a windows tun has no descriptor"));
}

} // namespace sihd::net
