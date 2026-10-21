#include <algorithm>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/sys/os.hpp>
#include <sihd/sys/windows/registry.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::windows_;

class TestRegistry: public ::testing::Test
{
    protected:
        TestRegistry() { LoggerManager::stream(); }

        virtual ~TestRegistry() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _path = fmt::format("Software\\sihd_test_{}", os::pid());
            auto created = _key.create(registry::Hive::current_user, _path);
            ASSERT_TRUE(created.has_value()) << created.error().message;
        }

        virtual void TearDown()
        {
            _key.close();
            (void)registry::remove(registry::Hive::current_user, _path);
        }

        std::string _path;
        registry::Key _key;
};

TEST_F(TestRegistry, test_string)
{
    ASSERT_TRUE(_key.set_string("name", "hello world").has_value());

    auto value = _key.string("name");
    ASSERT_TRUE(value.has_value()) << value.error().message;
    EXPECT_EQ(*value, "hello world");
}

TEST_F(TestRegistry, test_empty_string)
{
    ASSERT_TRUE(_key.set_string("empty", "").has_value());

    auto value = _key.string("empty");
    ASSERT_TRUE(value.has_value()) << value.error().message;
    EXPECT_EQ(*value, "");
}

TEST_F(TestRegistry, test_strings)
{
    ASSERT_TRUE(_key.set_strings("list", {"a", "bc", "def"}).has_value());

    auto values = _key.strings("list");
    ASSERT_TRUE(values.has_value()) << values.error().message;
    EXPECT_EQ(*values, (std::vector<std::string> {"a", "bc", "def"}));
}

TEST_F(TestRegistry, test_empty_strings)
{
    ASSERT_TRUE(_key.set_strings("list", {}).has_value());

    auto values = _key.strings("list");
    ASSERT_TRUE(values.has_value()) << values.error().message;
    EXPECT_EQ(*values, std::vector<std::string> {});
}

TEST_F(TestRegistry, test_integers)
{
    ASSERT_TRUE(_key.set_u32("dword", 0xdeadbeef).has_value());
    ASSERT_TRUE(_key.set_u64("qword", 0x1122334455667788).has_value());

    auto dword = _key.u32("dword");
    ASSERT_TRUE(dword.has_value()) << dword.error().message;
    EXPECT_EQ(*dword, 0xdeadbeefu);
    auto qword = _key.u64("qword");
    ASSERT_TRUE(qword.has_value()) << qword.error().message;
    EXPECT_EQ(*qword, 0x1122334455667788u);
}

TEST_F(TestRegistry, test_binary)
{
    const std::vector<int8_t> bytes {1, 2, 0, 3};
    ASSERT_TRUE(_key.set_binary("bin", bytes).has_value());

    auto value = _key.binary("bin");
    ASSERT_TRUE(value.has_value()) << value.error().message;
    EXPECT_EQ(*value, std::string("\x01\x02\x00\x03", 4));
}

TEST_F(TestRegistry, test_errors)
{
    ASSERT_TRUE(_key.set_u32("dword", 1).has_value());

    auto wrong_type = _key.string("dword");
    ASSERT_FALSE(wrong_type.has_value());
    EXPECT_EQ(wrong_type.error().code, ErrorCode::invalid_argument);

    auto missing_value = _key.u32("missing");
    ASSERT_FALSE(missing_value.has_value());
    EXPECT_EQ(missing_value.error().code, ErrorCode::not_found);

    registry::Key missing;
    auto opened = missing.open(registry::Hive::current_user, _path + "\\missing");
    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::not_found);
    EXPECT_FALSE(missing.is_open());
}

TEST_F(TestRegistry, test_enumeration)
{
    registry::Key child;
    ASSERT_TRUE(child.create(registry::Hive::current_user, _path + "\\b").has_value());
    ASSERT_TRUE(child.create(registry::Hive::current_user, _path + "\\a").has_value());
    ASSERT_TRUE(_key.set_u32("y", 1).has_value());
    ASSERT_TRUE(_key.set_u32("x", 2).has_value());

    auto subkeys = _key.subkeys();
    ASSERT_TRUE(subkeys.has_value()) << subkeys.error().message;
    std::ranges::sort(*subkeys);
    EXPECT_EQ(*subkeys, (std::vector<std::string> {"a", "b"}));
    auto names = _key.value_names();
    ASSERT_TRUE(names.has_value()) << names.error().message;
    std::ranges::sort(*names);
    EXPECT_EQ(*names, (std::vector<std::string> {"x", "y"}));
}

TEST_F(TestRegistry, test_remove)
{
    ASSERT_TRUE(_key.set_u32("dword", 1).has_value());
    registry::Key child;
    ASSERT_TRUE(child.create(registry::Hive::current_user, _path + "\\sub\\deep").has_value());
    child.close();

    ASSERT_TRUE(_key.remove_value("dword").has_value());
    auto removed_value = _key.u32("dword");
    ASSERT_FALSE(removed_value.has_value());
    EXPECT_EQ(removed_value.error().code, ErrorCode::not_found);

    auto removed = registry::remove(registry::Hive::current_user, _path + "\\sub");
    ASSERT_TRUE(removed.has_value()) << removed.error().message;
    auto opened = child.open(registry::Hive::current_user, _path + "\\sub");
    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::not_found);
}

TEST_F(TestRegistry, test_move)
{
    registry::Key moved(std::move(_key));
    EXPECT_TRUE(moved.is_open());
    EXPECT_FALSE(_key.is_open());

    _key = std::move(moved);
    EXPECT_TRUE(_key.is_open());
    EXPECT_FALSE(moved.is_open());
}

} // namespace test
