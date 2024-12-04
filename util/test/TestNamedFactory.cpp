#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/os.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestNamedFactory: public ::testing::Test
{
    protected:
        TestNamedFactory() { sihd::util::LoggerManager::stream(); }

        virtual ~TestNamedFactory() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestNamedFactory, test_namedfactory)
{
    if (os::is_run_with_asan)
        GTEST_SKIP() << "test does not work with address sanatizer";
    EXPECT_EQ(NamedFactory::load("unknown_lib", "symbol", "err"), nullptr);
    EXPECT_EQ(NamedFactory::load("sihd_util", "unknown_symbol", "err"), nullptr);

    Named *node = NamedFactory::load("sihd_util", "Node", "test_node");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->name(), "test_node");
    Node *casted = dynamic_cast<Node *>(node);
    ASSERT_NE(casted, nullptr);
    Named *child = NamedFactory::load("sihd_util", "Node", "child_node", casted);
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->parent(), casted);
    if (child->parent() != casted)
        delete child;
    delete node;
}
} // namespace test
