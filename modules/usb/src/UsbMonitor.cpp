#include <stdexcept>

#include <sihd/usb/UsbMonitor.hpp>
#include <sihd/util/time.hpp>

#include "libusb.hpp"

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::usb
{

struct UsbMonitor::Impl
{
        libusb::Context context;
        libusb_hotplug_callback_handle callback = 0;
        std::vector<UsbMonitorEvent> pending;

        static int LIBUSB_CALL on_hotplug(libusb_context *,
                                          libusb_device *device,
                                          libusb_hotplug_event event,
                                          void *data)
        {
            auto *self = static_cast<Impl *>(data);
            self->pending.emplace_back(UsbMonitorEvent {
                .type = event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED ? UsbMonitorEventType::arrived
                                                                     : UsbMonitorEventType::left,
                .device = libusb::to_device(device),
            });
            return 0;
        }

        ~Impl()
        {
            if (context != nullptr)
                libusb_hotplug_deregister_callback(context.get(), callback);
        }

        static Result<std::unique_ptr<Impl>> open()
        {
            if constexpr (!supported)
                return std::unexpected(Error(not_supported, "usb hotplug is not supported on this platform"));

            auto context = libusb::init_context();
            SIHD_UNEXPECTED_RETURN(context);
            auto impl = std::make_unique<Impl>();
            impl->context = std::move(*context);

            const int rc = libusb_hotplug_register_callback(
                impl->context.get(),
                LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                LIBUSB_HOTPLUG_ENUMERATE,
                LIBUSB_HOTPLUG_MATCH_ANY,
                LIBUSB_HOTPLUG_MATCH_ANY,
                LIBUSB_HOTPLUG_MATCH_ANY,
                &on_hotplug,
                impl.get(),
                &impl->callback);
            if (rc != LIBUSB_SUCCESS)
                return std::unexpected(libusb::to_error(rc, "register usb hotplug"));
            return impl;
        }
};

UsbMonitor::UsbMonitor()
{
    auto impl = Impl::open();
    if (!impl)
        throw std::runtime_error(impl.error().message);
    _impl = std::move(*impl);
}

UsbMonitor::UsbMonitor(Duration run_timeout): UsbMonitor()
{
    this->set_run_timeout(run_timeout);
}

UsbMonitor::~UsbMonitor() = default;

void UsbMonitor::set_run_timeout(Duration timeout)
{
    _run_timeout = timeout;
}

bool UsbMonitor::run()
{
    _events.clear();
    // the devices enumerated at construction are already queued: only poll for more
    const bool queued = !_impl->pending.empty();
    int rc;
    if (_run_timeout.nanoseconds() < 0 && !queued)
    {
        rc = libusb_handle_events_completed(_impl->context.get(), nullptr);
    }
    else
    {
        timeval tv = queued ? timeval {} : time::to_tv(time::to_micro(_run_timeout.nanoseconds()));
        rc = libusb_handle_events_timeout_completed(_impl->context.get(), &tv, nullptr);
    }
    // a signal ends the wait like its timeout: what arrived before it is still delivered
    if (rc != LIBUSB_SUCCESS && rc != LIBUSB_ERROR_INTERRUPTED)
        return false;

    _events = std::move(_impl->pending);
    _impl->pending.clear();
    if (!_events.empty())
        this->notify_observers(this);
    return true;
}

const std::vector<UsbMonitorEvent> & UsbMonitor::events() const
{
    return _events;
}

} // namespace sihd::usb
