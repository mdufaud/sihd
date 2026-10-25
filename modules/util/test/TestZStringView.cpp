#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

#include <sihd/util/ZStringView.hpp>

namespace test
{
using namespace sihd::util;

class TestZStringView: public ::testing::Test
{
    protected:
        TestZStringView() = default;
        virtual ~TestZStringView() = default;
};

TEST_F(TestZStringView, test_zstringview_literal_is_constexpr)
{
    constexpr ZStringView service = "org.freedesktop.NetworkManager";

    static_assert(service.size() == 30);
    EXPECT_EQ(service.view(), "org.freedesktop.NetworkManager");
    EXPECT_EQ(service.c_str()[service.size()], '\0');
}

TEST_F(TestZStringView, test_zstringview_from_string_shares_buffer)
{
    const std::string path = "/org/bluez/hci0";

    const ZStringView view = path;

    EXPECT_EQ(view.c_str(), path.c_str());
    EXPECT_EQ(view.size(), 15u);
    EXPECT_EQ(std::strlen(view.c_str()), 15u);
}

TEST_F(TestZStringView, test_zstringview_refuses_string_view)
{
    static_assert(!std::is_constructible_v<ZStringView, std::string_view>);
    static_assert(!std::is_constructible_v<ZStringView, std::nullptr_t>);
    static_assert(std::is_convertible_v<ZStringView, std::string_view>);
}

TEST_F(TestZStringView, test_zstringview_compares)
{
    const std::string interface = "org.bluez.Device1";
    constexpr ZStringView device_interface = "org.bluez.Device1";
    constexpr ZStringView adapter_interface = "org.bluez.Adapter1";

    EXPECT_TRUE(interface == device_interface);
    EXPECT_TRUE(std::string_view(interface) == device_interface);
    EXPECT_TRUE(device_interface == "org.bluez.Device1");
    EXPECT_FALSE(interface == adapter_interface);
    EXPECT_FALSE(device_interface == adapter_interface);
}

TEST_F(TestZStringView, test_zstringview_empty)
{
    constexpr ZStringView empty = "";

    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.c_str()[0], '\0');
}

} // namespace test
