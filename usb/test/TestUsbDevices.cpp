#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
// #include <sihd/usb/UsbDevices.hpp>

#include <libusb-1.0/libusb.h>

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
			return ;
		}
		printf("%04x:%04x (bus %d, device %d)",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(dev), libusb_get_device_address(dev));
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
            TestUsbDevices()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestUsbDevices()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
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
}