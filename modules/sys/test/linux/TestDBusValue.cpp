#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/linux/dbus/Value.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys::linux_::dbus;

class TestDBusValue: public ::testing::Test
{
    protected:
        TestDBusValue() { LoggerManager::stream(); }

        virtual ~TestDBusValue() { LoggerManager::clear_loggers(); }
};

TEST_F(TestDBusValue, test_scalar_signatures)
{
    EXPECT_EQ(Value(true).signature(), "b");
    EXPECT_EQ(Value(uint8_t {1}).signature(), "y");
    EXPECT_EQ(Value(int16_t {1}).signature(), "n");
    EXPECT_EQ(Value(uint16_t {1}).signature(), "q");
    EXPECT_EQ(Value(1).signature(), "i");
    EXPECT_EQ(Value(1u).signature(), "u");
    EXPECT_EQ(Value(int64_t {1}).signature(), "x");
    EXPECT_EQ(Value(uint64_t {1}).signature(), "t");
    EXPECT_EQ(Value(1.5).signature(), "d");
}

TEST_F(TestDBusValue, test_string_signatures)
{
    const std::string text = "text";

    EXPECT_EQ(Value("text").signature(), "s");
    EXPECT_EQ(Value(text).signature(), "s");
    EXPECT_EQ(Value(ZStringView(text)).signature(), "s");
    EXPECT_EQ(Value(ObjectPath("/org/sihd")).signature(), "o");
    EXPECT_EQ(Value(Signature("a{sv}")).signature(), "g");
}

TEST_F(TestDBusValue, test_container_signatures)
{
    const std::vector<std::vector<uint8_t>> frames {{1, 2}, {3}};
    const std::map<std::string, Dict> settings {{"802-11-wireless", {{"ssid", "home"}}}};

    EXPECT_EQ(Value(std::vector<std::string> {"a", "b"}).signature(), "as");
    EXPECT_EQ(Value(std::vector<uint32_t> {}).signature(), "au");
    EXPECT_EQ(Value(frames).signature(), "aay");
    EXPECT_EQ(Value(std::vector<ObjectPath> {}).signature(), "ao");
    EXPECT_EQ(Value(std::map<std::string, uint32_t> {{"a", 1}}).signature(), "a{su}");
    EXPECT_EQ(Value(settings).signature(), "a{sa{sv}}");
    EXPECT_EQ(Value(Struct {1u, "text"}).signature(), "(us)");
    EXPECT_EQ(Value(Variant(1u)).signature(), "v");
    EXPECT_EQ(Value::variant_dict(std::map<uint16_t, std::vector<uint8_t>> {{76, {1}}}).signature(), "a{qv}");
    using PathsByName = std::map<std::string, std::vector<ObjectPath>>;
    EXPECT_EQ(Value::signature_of<PathsByName>(), "a{sao}");
}

TEST_F(TestDBusValue, test_brace_pairs_make_a_dict)
{
    const Value options = {{"Role", "server"}, {"AutoConnect", false}};

    EXPECT_EQ(options.signature(), "a{sv}");
    EXPECT_EQ(Value(Dict {{"Role", "server"}}).signature(), "a{sv}");
    EXPECT_EQ(Value({}).signature(), "a{sv}");
    EXPECT_EQ((Value {{"a", "b"}}).signature(), "a{sv}");

    const auto & entries = std::get<Value::Array>(options.data()).items();
    ASSERT_EQ(entries.size(), 2u);
    const auto & entry = std::get<Value::DictEntry>(entries[1].data());
    EXPECT_EQ(std::get<std::string>(entry.key().data()), "AutoConnect");
    const auto & variant = std::get<Variant>(entry.value().data());
    EXPECT_EQ(std::get<bool>(variant.value().data()), false);
}

TEST_F(TestDBusValue, test_other_braces_make_an_array)
{
    const Value inner = 1u;

    EXPECT_EQ(Value({1u, 2u}).signature(), "au");
    EXPECT_EQ(Value({"a", "b"}).signature(), "as");
    EXPECT_EQ(Value {inner}.signature(), "au");
    EXPECT_EQ(Value(inner).signature(), "u");
}

} // namespace test
