#include <gtest/gtest.h>

#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/PluginLoader.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/build.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
class TestPluginLoader: public ::testing::Test
{
    protected:
        TestPluginLoader() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPluginLoader() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPluginLoader, test_pluginloader)
{
    if (sihd::util::build::is_run_with_sanitizer)
        GTEST_SKIP() << "test does not work with sanitizers";

    auto unknown_lib = PluginLoader::create_from_library("unknown_lib", "symbol", "err");
    ASSERT_FALSE(unknown_lib.has_value());
    if constexpr (DynLib::supported)
    {
        EXPECT_NE(unknown_lib.error().code, none);
    }

    auto unknown_factory = PluginLoader::create_from_library("sihd_util", "unknown_symbol", "err");
    ASSERT_FALSE(unknown_factory.has_value());
    if constexpr (DynLib::supported)
    {
        EXPECT_NE(unknown_factory.error().code, none);
    }

    if constexpr (!sihd::util::build::is_statically_linked)
    {
        // plugin loading needs a dynamic build — static links have no loadable module
        auto loaded = PluginLoader::create_from_library("sihd_util", "Node", "test_node");
        ASSERT_TRUE(loaded.has_value());
        Named *node = loaded.value();
        EXPECT_EQ(node->name(), "test_node");
        Node *casted = dynamic_cast<Node *>(node);
        ASSERT_NE(casted, nullptr);
        auto loaded_child = PluginLoader::create_from_library("sihd_util", "Node", "child_node", casted);
        ASSERT_TRUE(loaded_child.has_value());
        Named *child = loaded_child.value();
        EXPECT_EQ(child->parent(), casted);
        if (child->parent() != casted)
            delete child;
        delete node;
    }
}

TEST_F(TestPluginLoader, test_pluginloader_create_from_linked)
{
    auto unknown = PluginLoader::create_from_linked("Nope", "nope_obj");
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);

    auto loaded = PluginLoader::create_from_linked("LoggerFile", "linked_file");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded.value()->name(), "linked_file");
    delete loaded.value();
}
} // namespace test
