#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>
// #include <sihd/usb/UsbDevices.hpp>

#include <libusb-1.0/libusb.h>

#include <fcntl.h>
#include <libudev.h>

static struct udev_hwdb *hwdb = NULL;

static const char *hwdb_get(const char *modalias, const char *key)
{
    struct udev_list_entry *entry;

    udev_list_entry_foreach(
        entry,
        udev_hwdb_get_properties_list_entry(hwdb,
                                            modalias,
                                            0)) if (strcmp(udev_list_entry_get_name(entry), key)
                                                    == 0) return udev_list_entry_get_value(entry);

    return NULL;
}

static const char *names_vendor(uint16_t vendorid)
{
    char modalias[64];

    sprintf(modalias, "usb:v%04X*", vendorid);
    return hwdb_get(modalias, "ID_VENDOR_FROM_DATABASE");
}

const char *names_product(uint16_t vendorid, uint16_t productid)
{
    char modalias[64];

    sprintf(modalias, "usb:v%04Xp%04X*", vendorid, productid);
    return hwdb_get(modalias, "ID_MODEL_FROM_DATABASE");
}

static int get_product_string(char *buf, size_t size, uint16_t vid, uint16_t pid)
{
    const char *cp;

    if (size < 1)
        return 0;
    *buf = 0;
    if (!(cp = names_product(vid, pid)))
        return 0;
    return snprintf(buf, size, "%s", cp);
}

static int get_vendor_string(char *buf, size_t size, uint16_t vid)
{
    const char *cp;

    if (size < 1)
        return 0;
    *buf = 0;
    if (!(cp = names_vendor(vid)))
        return 0;
    return snprintf(buf, size, "%s", cp);
}

const char *names_class(uint8_t classid)
{
    char modalias[64];

    sprintf(modalias, "usb:v*p*d*dc%02X*", classid);
    return hwdb_get(modalias, "ID_USB_CLASS_FROM_DATABASE");
}

const char *names_subclass(uint8_t classid, uint8_t subclassid)
{
    char modalias[64];

    sprintf(modalias, "usb:v*p*d*dc%02Xdsc%02X*", classid, subclassid);
    return hwdb_get(modalias, "ID_USB_SUBCLASS_FROM_DATABASE");
}

int get_class_string(char *buf, size_t size, uint8_t cls)
{
    const char *cp;

    if (size < 1)
        return 0;
    *buf = 0;
    if (!(cp = names_class(cls)))
        return snprintf(buf, size, "[unknown]");
    return snprintf(buf, size, "%s", cp);
}

int get_subclass_string(char *buf, size_t size, uint8_t cls, uint8_t subcls)
{
    const char *cp;

    if (size < 1)
        return 0;
    *buf = 0;
    if (!(cp = names_subclass(cls, subcls)))
        return snprintf(buf, size, "[unknown]");
    return snprintf(buf, size, "%s", cp);
}

int get_sysfs_name(char *buf, size_t size, libusb_device *dev)
{
    int len = 0;
    uint8_t bnum = libusb_get_bus_number(dev);
    uint8_t pnums[7];
    int num_pnums;

    buf[0] = '\0';

    num_pnums = libusb_get_port_numbers(dev, pnums, sizeof(pnums));
    if (num_pnums == LIBUSB_ERROR_OVERFLOW)
    {
        return -1;
    }
    else if (num_pnums == 0)
    {
        /* Special-case root devices */
        return snprintf(buf, size, "usb%d", bnum);
    }

    len += snprintf(buf, size, "%d-", bnum);
    for (int i = 0; i < num_pnums; i++)
        len += snprintf(buf + len, size - len, i ? ".%d" : "%d", pnums[i]);

    return len;
}

int read_sysfs_prop(char *buf, size_t size, char *sysfs_name, const char *propname)
{
    int n, fd;
    char path[PATH_MAX];

    buf[0] = '\0';
    snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/%s", sysfs_name, propname);
    fd = open(path, O_RDONLY);

    if (fd == -1)
        return 0;

    n = read(fd, buf, size);

    if (n > 0)
        buf[n - 1] = '\0'; // Turn newline into null terminator

    close(fd);
    return n;
}

static void get_vendor_product_with_fallback(char *vendor,
                                             int vendor_len,
                                             char *product,
                                             int product_len,
                                             libusb_device *dev)
{
    struct libusb_device_descriptor desc;
    char sysfs_name[PATH_MAX];
    bool have_vendor, have_product;

    libusb_get_device_descriptor(dev, &desc);

    /* set to "[unknown]" by default unless something below finds a string */
    strncpy(vendor, "[unknown]", vendor_len);
    strncpy(product, "[unknown]", product_len);

    have_vendor = !!get_vendor_string(vendor, vendor_len, desc.idVendor);
    have_product = !!get_product_string(product, product_len, desc.idVendor, desc.idProduct);

    if (have_vendor && have_product)
        return;

    if (get_sysfs_name(sysfs_name, sizeof(sysfs_name), dev) >= 0)
    {
        if (!have_vendor)
            read_sysfs_prop(vendor, vendor_len, sysfs_name, "manufacturer");
        if (!have_product)
            read_sysfs_prop(product, product_len, sysfs_name, "product");
    }
}

const char *names_protocol(uint8_t classid, uint8_t subclassid, uint8_t protocolid)
{
    char modalias[64];

    sprintf(modalias, "usb:v*p*d*dc%02Xdsc%02Xdp%02X*", classid, subclassid, protocolid);
    return hwdb_get(modalias, "ID_USB_PROTOCOL_FROM_DATABASE");
}

static int get_protocol_string(char *buf, size_t size, uint8_t cls, uint8_t subcls, uint8_t proto)
{
    const char *cp;

    if (size < 1)
        return 0;
    *buf = 0;
    if (!(cp = names_protocol(cls, subcls, proto)))
        return 0;
    return snprintf(buf, size, "%s", cp);
}

static void dump_device(libusb_device *dev, struct libusb_device_descriptor *descriptor)
{
    char vendor[128], product[128];
    char cls[128], subcls[128], proto[128];
    char mfg[128] = {0}, prod[128] = {0}, serial[128] = {0};
    char sysfs_name[PATH_MAX];

    get_vendor_product_with_fallback(vendor, sizeof(vendor), product, sizeof(product), dev);
    get_class_string(cls, sizeof(cls), descriptor->bDeviceClass);
    get_subclass_string(subcls, sizeof(subcls), descriptor->bDeviceClass, descriptor->bDeviceSubClass);
    get_protocol_string(proto,
                        sizeof(proto),
                        descriptor->bDeviceClass,
                        descriptor->bDeviceSubClass,
                        descriptor->bDeviceProtocol);

    if (get_sysfs_name(sysfs_name, sizeof(sysfs_name), dev) >= 0)
    {
        read_sysfs_prop(mfg, sizeof(mfg), sysfs_name, "manufacturer");
        read_sysfs_prop(prod, sizeof(prod), sysfs_name, "product");
        read_sysfs_prop(serial, sizeof(serial), sysfs_name, "serial");
    }

    printf("Device Descriptor:\n"
           "  bLength             %5u\n"
           "  bDescriptorType     %5u\n"
           "  bcdUSB              %2x.%02x\n"
           "  bDeviceClass        %5u %s\n"
           "  bDeviceSubClass     %5u %s\n"
           "  bDeviceProtocol     %5u %s\n"
           "  bMaxPacketSize0     %5u\n"
           "  idVendor           0x%04x %s\n"
           "  idProduct          0x%04x %s\n"
           "  bcdDevice           %2x.%02x\n"
           "  iManufacturer       %5u %s\n"
           "  iProduct            %5u %s\n"
           "  iSerial             %5u %s\n"
           "  bNumConfigurations  %5u\n",
           descriptor->bLength,
           descriptor->bDescriptorType,
           descriptor->bcdUSB >> 8,
           descriptor->bcdUSB & 0xff,
           descriptor->bDeviceClass,
           cls,
           descriptor->bDeviceSubClass,
           subcls,
           descriptor->bDeviceProtocol,
           proto,
           descriptor->bMaxPacketSize0,
           descriptor->idVendor,
           vendor,
           descriptor->idProduct,
           product,
           descriptor->bcdDevice >> 8,
           descriptor->bcdDevice & 0xff,
           descriptor->iManufacturer,
           mfg,
           descriptor->iProduct,
           prod,
           descriptor->iSerialNumber,
           serial,
           descriptor->bNumConfigurations);
}

static void print_devs(libusb_device **devs)
{
    libusb_device *dev;
    int i = 0, j = 0;
    uint8_t path[8];

    while ((dev = devs[i++]) != NULL)
    {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0)
        {
            fprintf(stderr, "failed to get device descriptor");
            return;
        }
        dump_device(dev, &desc);
        r = libusb_get_port_numbers(dev, path, sizeof(path));
        if (r > 0)
        {
            printf(" path: %d", path[0]);
            for (j = 1; j < r; j++)
                printf(".%d", path[j]);
        }
        printf("\n");
    }
}

namespace test
{
SIHD_LOGGER;
// using namespace sihd::usb;
using namespace sihd::util;
class TestUsbDevices: public ::testing::Test
{
    protected:
        TestUsbDevices() { sihd::util::LoggerManager::stream(); }

        virtual ~TestUsbDevices() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestUsbDevices, test_usbdevices)
{
    libusb_device **devs;
    int r;
    ssize_t cnt;

    r = libusb_init(NULL);
    ASSERT_GE(r, 0);

    cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0)
        libusb_exit(NULL);
    ASSERT_GE(cnt, 0);

    print_devs(devs);
    libusb_free_device_list(devs, 1);

    libusb_exit(NULL);
}
} // namespace test