#include "FakeUsbDevice.hpp"

#if __has_include(<umockdev.h>)

# include <sys/ioctl.h>
# include <umockdev.h>

# include <algorithm>
# include <cerrno>
# include <cstring>
# include <deque>
# include <map>
# include <mutex>
# include <set>
# include <string>
# include <vector>

# include <fmt/format.h>

# include <sihd/sys/fs.hpp>

# include <linux/usbdevice_fs.h>

namespace test
{

namespace
{

// GET_DESCRIPTOR and GET_CONFIGURATION: standard requests 6 and 8, descriptor type in the high byte of value
constexpr uint8_t get_descriptor = 6;
constexpr uint8_t get_configuration = 8;
constexpr uint8_t device_type = 1;
constexpr uint8_t configuration_type = 2;
constexpr uint8_t string_type = 3;
// the 8 setup bytes lead the buffer of a control urb
constexpr size_t setup_size = 8;
// usbfs GET_SPEED answer: USB_SPEED_FULL
constexpr int full_speed = 2;

std::vector<uint8_t> descriptors(uint16_t product)
{
    const uint8_t vendor_low = FakeUsbDevice::vendor_id & 0xff;
    const uint8_t vendor_high = FakeUsbDevice::vendor_id >> 8;
    const uint8_t product_low = product & 0xff;
    const uint8_t product_high = product >> 8;
    // clang-format off
    return {
        // device: usb 2.00, max packet 64, vendor, product, release 1.00, strings 1 and 2, no serial, 1 configuration
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, vendor_low, vendor_high, product_low, product_high,
        0x00, 0x01, 0x01, 0x02, 0x00, 0x01,
        // configuration: 34 bytes in all, 1 interface, value 1, bus powered with remote wakeup, 50 units of 2 mA
        0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xa0, 0x32,
        // interface 0 alt 0: 1 endpoint, class hid (3), boot subclass (1), keyboard protocol (1)
        0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,
        // hid 1.11: no country, 1 report descriptor of 63 bytes
        0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x3f, 0x00,
        // endpoint 0x81: interrupt in, 8 bytes, every 10 ms
        0x07, 0x05, FakeUsbDevice::interrupt_endpoint, 0x03, 0x08, 0x00, 0x0a,
    };
    // clang-format on
}

std::vector<uint8_t> string_descriptor(uint8_t index)
{
    // index 0 lists the languages: en-US (0x0409)
    if (index == 0)
        return {0x04, string_type, 0x09, 0x04};
    const std::string_view text = index == 1 ? FakeUsbDevice::manufacturer : FakeUsbDevice::product;
    std::vector<uint8_t> out {static_cast<uint8_t>(2 + text.size() * 2), string_type};
    for (const char c : text)
    {
        out.push_back(static_cast<uint8_t>(c));
        out.push_back(0);
    }
    return out;
}

// the device record umockdev reads: sysfs attributes then the usbfs node
std::string device_record(const std::string & port, uint8_t address, uint16_t product)
{
    std::string hex;
    for (const uint8_t byte : descriptors(product))
        hex += fmt::format("{:02x}", byte);
    return fmt::format("P: /devices/usb1/{0}\n"
                       "N: bus/usb/001/{1:03}\n"
                       "E: SUBSYSTEM=usb\n"
                       "E: DEVTYPE=usb_device\n"
                       "E: BUSNUM=001\n"
                       "E: DEVNUM={1:03}\n"
                       "E: DEVNAME=/dev/bus/usb/001/{1:03}\n"
                       "A: busnum=1\\n\n"
                       "A: devnum={1}\\n\n"
                       "A: speed=12\\n\n"
                       "A: bConfigurationValue=1\\n\n"
                       "A: bMaxPower=100mA\\n\n"
                       "H: descriptors={2}\n",
                       port,
                       address,
                       hex);
}

} // namespace

struct FakeUsbDevice::Impl
{
        UMockdevTestbed *testbed = nullptr;
        UMockdevIoctlBase *handler = nullptr;
        std::mutex mutex;
        // urbs answered, waiting for their reap
        std::deque<UMockdevIoctlData *> done;
        // held urbs, answered when cancelled
        std::vector<UMockdevIoctlData *> pending;
        // read offset of each opened node, libusb reads the descriptors after a wrap
        std::map<UMockdevIoctlClient *, size_t> positions;
        std::set<unsigned int> claimed;
        int interrupt_status = 0;
        bool hold = false;
        int held_size = 0;

        ~Impl()
        {
            for (UMockdevIoctlData *urb : done)
                umockdev_ioctl_data_unref(urb);
            for (UMockdevIoctlData *urb : pending)
                umockdev_ioctl_data_unref(urb);
            if (handler != nullptr)
                g_object_unref(handler);
            if (testbed != nullptr)
                g_object_unref(testbed);
        }

        static std::vector<uint8_t> control_reply(uint8_t request, uint16_t value)
        {
            const uint8_t type = value >> 8;
            const uint8_t index = value & 0xff;
            if (request == get_configuration)
                return {0x01};
            if (request != get_descriptor)
                return {};
            const std::vector<uint8_t> all = descriptors(product_id);
            if (type == device_type)
                return {all.begin(), all.begin() + 18};
            if (type == configuration_type)
                return {all.begin() + 18, all.end()};
            if (type == string_type)
                return string_descriptor(index);
            return {};
        }

        void answer(usbdevfs_urb *urb)
        {
            auto *buffer = static_cast<uint8_t *>(urb->buffer);
            urb->status = 0;
            urb->actual_length = 0;

            if (urb->type == USBDEVFS_URB_TYPE_CONTROL)
            {
                // setup: bmRequestType at 0, bRequest at 1, wValue little endian at 2
                const uint16_t value = static_cast<uint16_t>(buffer[2] | (buffer[3] << 8));
                const std::vector<uint8_t> reply = control_reply(buffer[1], value);
                const size_t size = std::min(static_cast<size_t>(urb->buffer_length) - setup_size, reply.size());
                std::memcpy(buffer + setup_size, reply.data(), size);
                urb->actual_length = static_cast<int>(size);
            }
            else if (urb->type == USBDEVFS_URB_TYPE_INTERRUPT && urb->endpoint == interrupt_endpoint)
            {
                urb->status = interrupt_status;
                if (interrupt_status == 0)
                {
                    const size_t size = std::min(static_cast<size_t>(urb->buffer_length), report.size());
                    std::memcpy(buffer, report.data(), size);
                    urb->actual_length = static_cast<int>(size);
                }
            }
        }

        void submit(UMockdevIoctlClient *client, UMockdevIoctlData *arg)
        {
            UMockdevIoctlData *urb_data = umockdev_ioctl_data_resolve(arg, 0, sizeof(usbdevfs_urb), nullptr);
            auto *urb = reinterpret_cast<usbdevfs_urb *>(urb_data->data);
            g_autoptr(UMockdevIoctlData) buffer = umockdev_ioctl_data_resolve(urb_data,
                                                                              G_STRUCT_OFFSET(usbdevfs_urb, buffer),
                                                                              urb->buffer_length,
                                                                              nullptr);
            std::lock_guard lock(mutex);
            if (hold && urb->type == USBDEVFS_URB_TYPE_INTERRUPT)
            {
                pending.push_back(urb_data);
            }
            else
            {
                this->answer(urb);
                done.push_back(urb_data);
            }
            umockdev_ioctl_client_complete(client, 0, 0);
        }

        void reap(UMockdevIoctlClient *client, UMockdevIoctlData *arg)
        {
            UMockdevIoctlData *urb_data = nullptr;
            {
                std::lock_guard lock(mutex);
                if (!done.empty())
                {
                    urb_data = done.front();
                    done.pop_front();
                }
            }
            if (urb_data == nullptr)
            {
                umockdev_ioctl_client_complete(client, -1, EAGAIN);
                return;
            }
            g_autoptr(UMockdevIoctlData) pointer = umockdev_ioctl_data_resolve(arg, 0, sizeof(gpointer), nullptr);
            umockdev_ioctl_data_set_ptr(pointer, 0, urb_data);
            umockdev_ioctl_data_unref(urb_data);
            umockdev_ioctl_client_complete(client, 0, 0);
        }

        void discard(UMockdevIoctlClient *client, UMockdevIoctlData *arg)
        {
            const auto address = reinterpret_cast<gulong>(*reinterpret_cast<void **>(arg->data));
            std::lock_guard lock(mutex);
            const auto held = std::ranges::find(pending, address, &UMockdevIoctlData::client_addr);
            if (held == pending.end())
            {
                umockdev_ioctl_client_complete(client, -1, EINVAL);
                return;
            }

            auto *urb = reinterpret_cast<usbdevfs_urb *>((*held)->data);
            const size_t size = std::min(static_cast<size_t>(urb->buffer_length), static_cast<size_t>(held_size));
            std::memcpy(urb->buffer, report.data(), std::min(size, report.size()));
            urb->actual_length = static_cast<int>(size);
            urb->status = -ENOENT;

            done.push_back(*held);
            pending.erase(held);
            umockdev_ioctl_client_complete(client, 0, 0);
        }

        void control(UMockdevIoctlClient *client, UMockdevIoctlData *arg)
        {
            g_autoptr(UMockdevIoctlData)
                transfer_data = umockdev_ioctl_data_resolve(arg, 0, sizeof(usbdevfs_ctrltransfer), nullptr);
            auto *transfer = reinterpret_cast<usbdevfs_ctrltransfer *>(transfer_data->data);
            g_autoptr(UMockdevIoctlData)
                data = umockdev_ioctl_data_resolve(transfer_data,
                                                   G_STRUCT_OFFSET(usbdevfs_ctrltransfer, data),
                                                   transfer->wLength,
                                                   nullptr);
            const std::vector<uint8_t> reply = control_reply(transfer->bRequest, transfer->wValue);
            const size_t size = std::min(static_cast<size_t>(transfer->wLength), reply.size());
            std::memcpy(data->data, reply.data(), size);
            umockdev_ioctl_client_complete(client, static_cast<glong>(size), 0);
        }

        void set_claimed(UMockdevIoctlClient *client, UMockdevIoctlData *arg, bool value)
        {
            g_autoptr(UMockdevIoctlData) number = umockdev_ioctl_data_resolve(arg, 0, sizeof(unsigned int), nullptr);
            const unsigned int interface_number = *reinterpret_cast<unsigned int *>(number->data);
            {
                std::lock_guard lock(mutex);
                if (value)
                    claimed.insert(interface_number);
                else
                    claimed.erase(interface_number);
            }
            umockdev_ioctl_client_complete(client, 0, 0);
        }

        // umockdev calls the handlers from its worker thread
        static gboolean on_ioctl(UMockdevIoctlBase *, UMockdevIoctlClient *client, gpointer data)
        {
            auto *self = static_cast<Impl *>(data);
            UMockdevIoctlData *arg = umockdev_ioctl_client_get_arg(client);
            switch (umockdev_ioctl_client_get_request(client))
            {
                case USBDEVFS_GET_CAPABILITIES:
                {
                    g_autoptr(UMockdevIoctlData)
                        capabilities = umockdev_ioctl_data_resolve(arg, 0, sizeof(guint32), nullptr);
                    *reinterpret_cast<guint32 *>(capabilities->data) = USBDEVFS_CAP_ZERO_PACKET
                                                                       | USBDEVFS_CAP_BULK_CONTINUATION;
                    umockdev_ioctl_client_complete(client, 0, 0);
                    return TRUE;
                }
                case USBDEVFS_CONNECTINFO:
                {
                    g_autoptr(UMockdevIoctlData)
                        info_data = umockdev_ioctl_data_resolve(arg, 0, sizeof(usbdevfs_connectinfo), nullptr);
                    auto *info = reinterpret_cast<usbdevfs_connectinfo *>(info_data->data);
                    info->devnum = 2;
                    info->slow = 0;
                    umockdev_ioctl_client_complete(client, 0, 0);
                    return TRUE;
                }
                case USBDEVFS_GET_SPEED:
                    umockdev_ioctl_client_complete(client, full_speed, 0);
                    return TRUE;
                case USBDEVFS_CONTROL:
                    self->control(client, arg);
                    return TRUE;
                case USBDEVFS_SUBMITURB:
                    self->submit(client, arg);
                    return TRUE;
                case USBDEVFS_REAPURB:
                case USBDEVFS_REAPURBNDELAY:
                    self->reap(client, arg);
                    return TRUE;
                case USBDEVFS_DISCARDURB:
                    self->discard(client, arg);
                    return TRUE;
                case USBDEVFS_CLAIMINTERFACE:
                    self->set_claimed(client, arg, true);
                    return TRUE;
                case USBDEVFS_RELEASEINTERFACE:
                    self->set_claimed(client, arg, false);
                    return TRUE;
                case USBDEVFS_SETINTERFACE:
                case USBDEVFS_SETCONFIGURATION:
                case USBDEVFS_CLEAR_HALT:
                case USBDEVFS_RESET:
                    umockdev_ioctl_client_complete(client, 0, 0);
                    return TRUE;
                default:
                    return FALSE;
            }
        }

        static gboolean on_read(UMockdevIoctlBase *, UMockdevIoctlClient *client, gpointer data)
        {
            auto *self = static_cast<Impl *>(data);
            UMockdevIoctlData *buffer = umockdev_ioctl_client_get_arg(client);
            const std::vector<uint8_t> all = descriptors(product_id);
            std::lock_guard lock(self->mutex);
            size_t & position = self->positions[client];
            const size_t size = std::min(static_cast<size_t>(buffer->data_len), all.size() - position);
            std::memcpy(buffer->data, all.data() + position, size);
            position += size;
            umockdev_ioctl_client_complete(client, static_cast<glong>(size), 0);
            return TRUE;
        }
};

FakeUsbDevice::FakeUsbDevice(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

FakeUsbDevice::~FakeUsbDevice() = default;

std::unique_ptr<FakeUsbDevice> FakeUsbDevice::create()
{
    auto impl = std::make_unique<Impl>();
    impl->testbed = umockdev_testbed_new();
    // true only once a testbed redirects /sys, and cached: asked after the testbed exists
    if (!umockdev_in_mock_environment())
        return nullptr;

    impl->handler = umockdev_ioctl_base_new();
    g_signal_connect(impl->handler, "handle-ioctl", G_CALLBACK(&Impl::on_ioctl), impl.get());
    g_signal_connect(impl->handler, "handle-read", G_CALLBACK(&Impl::on_read), impl.get());
    if (!umockdev_testbed_attach_ioctl(impl->testbed, "/dev/bus/usb/001/002", impl->handler, nullptr))
        return nullptr;
    const std::string records = device_record("1-1", 2, product_id) + "\n"
                                + device_record("1-2", 3, read_only_product_id);
    if (!umockdev_testbed_add_from_string(impl->testbed, records.c_str(), nullptr))
        return nullptr;

    const std::string read_only_node = fmt::format("{}/dev/bus/usb/001/003",
                                                   umockdev_testbed_get_root_dir(impl->testbed));
    if (!sihd::sys::fs::permission_set(read_only_node, 0444))
        return nullptr;
    return std::unique_ptr<FakeUsbDevice>(new FakeUsbDevice(std::move(impl)));
}

void FakeUsbDevice::set_interrupt_status(int status)
{
    std::lock_guard lock(_impl->mutex);
    _impl->interrupt_status = status;
}

void FakeUsbDevice::hold_interrupt(int size)
{
    std::lock_guard lock(_impl->mutex);
    _impl->hold = true;
    _impl->held_size = size;
}

bool FakeUsbDevice::is_claimed(uint8_t interface_number) const
{
    std::lock_guard lock(_impl->mutex);
    return _impl->claimed.contains(interface_number);
}

} // namespace test

#else

namespace test
{

struct FakeUsbDevice::Impl
{
};

FakeUsbDevice::FakeUsbDevice(std::unique_ptr<Impl> impl): _impl(std::move(impl)) {}

FakeUsbDevice::~FakeUsbDevice() = default;

std::unique_ptr<FakeUsbDevice> FakeUsbDevice::create()
{
    return nullptr;
}

void FakeUsbDevice::set_interrupt_status(int) {}

void FakeUsbDevice::hold_interrupt(int) {}

bool FakeUsbDevice::is_claimed(uint8_t) const
{
    return false;
}

} // namespace test

#endif
