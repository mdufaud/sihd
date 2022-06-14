#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
// #include <sihd/bt/BtScan.hpp>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

namespace test
{
    SIHD_LOGGER;
    // using namespace sihd::bt;
    using namespace sihd::util;
    class TestBtScan: public ::testing::Test
    {
        protected:
            TestBtScan()
            {
                sihd::util::LoggerManager::basic();
                // char *test_path = getenv("TEST_PATH");
                // _base_test_dir = sihd::util::FS::combine({
                //     test_path == nullptr ? "unit_test" : test_path,
                //     "bt",
                //     "btscan"
                // });
                // _cwd = sihd::util::OS::cwd();
                // sihd::util::FS::make_directories(_base_test_dir);
            }

            virtual ~TestBtScan()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            // std::string _cwd;
            // std::string _base_test_dir;
    };

    TEST_F(TestBtScan, test_btscan)
    {
        inquiry_info *ii = NULL;
        int max_rsp, num_rsp;
        int dev_id, sock, len, flags;
        int i;
        char addr[19] = { 0 };
        char name[248] = { 0 };

        dev_id = hci_get_route(NULL);
        sock = hci_open_dev(dev_id);
        if (dev_id < 0 || sock < 0)
        {
            perror("opening socket");
            exit(1);
        }

        len = 8;
        max_rsp = 255;
        flags = IREQ_CACHE_FLUSH;
        ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
        num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
        if (num_rsp < 0)
            perror("hci_inquiry");

        for (i = 0; i < num_rsp; i++)
        {
            ba2str(&(ii + i)->bdaddr, addr);
            memset(name, 0, sizeof(name));
            if (hci_read_remote_name(sock, &(ii + i)->bdaddr, sizeof(name), name, 0) < 0)
                strcpy(name, "[unknown]");
            printf("%s  %s\n", addr, name);
        }
        free(ii);
        close(sock);
    }

    // TEST_F(TestBtScan, test_btscan_interactive)
    // {
    //     if (sihd::util::Term::is_interactive() == false)
    //         GTEST_SKIP_("requires interaction");
    // }

    // TEST_F(TestBtScan, test_btscan_file)
    // {
    //     std::string test_dir = sihd::util::FS::combine(_base_test_dir, "file");
    //     sihd::util::FS::remove_directories(test_dir);
    //     sihd::util::FS::make_directories(test_dir);
    // }
}