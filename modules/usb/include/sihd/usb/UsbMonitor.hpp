#ifndef __SIHD_USB_USBMONITOR_HPP__
#define __SIHD_USB_USBMONITOR_HPP__

#include <chrono>
#include <memory>
#include <vector>

#include <sihd/usb/UsbDevice.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

namespace sihd::usb
{

enum class UsbMonitorEventType
{
    arrived,
    left,
};

struct UsbMonitorEvent
{
        UsbMonitorEventType type = UsbMonitorEventType::arrived;
        UsbDevice device;
};

class UsbMonitor: public sihd::util::Observable<UsbMonitor>,
                  public sihd::util::IRunnable
{
    public:
        // libusb has no hotplug on windows, an android app cannot list devices
        static constexpr bool supported = sihd::util::build::is_linux && !sihd::util::build::is_android;

        UsbMonitor();
        explicit UsbMonitor(sihd::util::Duration run_timeout);
        virtual ~UsbMonitor();

        // subscribes to hotplug: the devices already plugged arrive at the first run
        sihd::util::Result<void> open();
        bool is_open() const;

        // a negative timeout waits for a change
        void set_run_timeout(sihd::util::Duration timeout);

        // false until opened
        bool run() override;

        const std::vector<UsbMonitorEvent> & events() const;

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
        sihd::util::Duration _run_timeout = std::chrono::milliseconds(100);
        std::vector<UsbMonitorEvent> _events;
};

} // namespace sihd::usb

#endif
