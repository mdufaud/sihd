#include <gtest/gtest.h>

#include <sihd/bt/Advertiser.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;

class TestBtAdvertiser: public ::testing::Test
{
    protected:
        TestBtAdvertiser(): _advertiser("advertiser") { LoggerManager::stream(); }

        virtual ~TestBtAdvertiser() { LoggerManager::clear_loggers(); }

        Advertiser _advertiser;
};

TEST_F(TestBtAdvertiser, test_gatt_server_fields_refused)
{
    const Advertisement named {.local_name = "sihd",
                               .service_uuids = {},
                               .manufacturer_data = {},
                               .service_data = {},
                               .connectable = false};
    const Advertisement with_uuids {.local_name = {},
                                    .service_uuids = {"0000180d-0000-1000-8000-00805f9b34fb"},
                                    .manufacturer_data = {},
                                    .service_data = {},
                                    .connectable = false};
    const Advertisement connectable {.local_name = {},
                                     .service_uuids = {},
                                     .manufacturer_data = {},
                                     .service_data = {},
                                     .connectable = true};

    ASSERT_TRUE(_advertiser.set_advertisement(named));
    EXPECT_FALSE(_advertiser.start());
    ASSERT_TRUE(_advertiser.set_advertisement(with_uuids));
    EXPECT_FALSE(_advertiser.start());
    ASSERT_TRUE(_advertiser.set_advertisement(connectable));
    EXPECT_FALSE(_advertiser.start());
}

TEST_F(TestBtAdvertiser, test_other_adapter_refused)
{
    ASSERT_TRUE(_advertiser.set_adapter(1));
    ASSERT_TRUE(_advertiser.set_advertisement(Advertisement {.local_name = {},
                                                             .service_uuids = {},
                                                             .manufacturer_data = {{0xffff, {0x01}}},
                                                             .service_data = {},
                                                             .connectable = false}));

    EXPECT_FALSE(_advertiser.start());
}

} // namespace test
