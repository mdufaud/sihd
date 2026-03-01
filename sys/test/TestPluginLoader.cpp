#include <gtest/gtest.h>
#include <sihd/sys/PluginLoader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/platform.hpp>

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
    if (sihd::util::platform::is_run_with_asan)
        GTEST_SKIP() << "test does not work with address sanitizer";
    EXPECT_EQ(PluginLoader::load("unknown_lib", "symbol", "err"), nullptr);
    EXPECT_EQ(PluginLoader::load("sihd_util", "unknown_symbol", "err"), nullptr);

    Named *node = PluginLoader::load("sihd_sys", "Node", "test_node");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->name(), "test_node");
    Node *casted = dynamic_cast<Node *>(node);
    ASSERT_NE(casted, nullptr);
    Named *child = PluginLoader::load("sihd_sys", "Node", "child_node", casted);
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->parent(), casted);
    if (child->parent() != casted)
        delete child;
    delete node;
}
} // namespace test
