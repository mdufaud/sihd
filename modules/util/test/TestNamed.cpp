#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestNamed: public ::testing::Test
{
    protected:
        TestNamed() { sihd::util::LoggerManager::stream(); }

        virtual ~TestNamed() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestNamed, test_named_util)
{
    Named named("obj");
    EXPECT_EQ(named.name(), "obj");
    EXPECT_EQ(named.parent(), nullptr);
}

TEST_F(TestNamed, test_named_find)
{
    Named named("obj");
    EXPECT_EQ(named.find("."), &named);
    EXPECT_EQ(named.find<Node>("."), nullptr);
    EXPECT_EQ(named.find(".."), nullptr);
    EXPECT_EQ(named.find("/"), nullptr);
}

TEST_F(TestNamed, test_named_full_name)
{
    Node root("root");
    Node *child = root.add_child<Node>("child");
    Named *leaf = child->add_child<Named>("leaf");

    EXPECT_EQ(root.full_name(), "root");
    EXPECT_EQ(child->full_name(), "root.child");
    EXPECT_EQ(leaf->full_name(), "root.child.leaf");
    EXPECT_FALSE(leaf->class_name().empty());
    EXPECT_NE(leaf->class_name().find("Named"), std::string::npos);
}

TEST_F(TestNamed, test_named_ownership)
{
    Node root("root");
    Named *child = new Named("child");

    EXPECT_FALSE(child->is_owned_by_parent());
    root.add_child(child, true);
    EXPECT_TRUE(child->is_owned_by_parent());

    Named *unowned = new Named("unowned");
    root.add_child(unowned, false);
    EXPECT_FALSE(unowned->is_owned_by_parent());
    // cleanup: unowned is not deleted by root
    root.remove_child(unowned);
    delete unowned;
}

TEST_F(TestNamed, test_named_find_all)
{
    Node root("root");
    root.add_child<Named>("a");
    root.add_child<Named>("b");
    root.add_child<Named>("c");

    auto [a, b, c] = root.find_all("a", "b", "c");
    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_NE(c, nullptr);
    EXPECT_EQ(a->name(), "a");
    EXPECT_EQ(b->name(), "b");
    EXPECT_EQ(c->name(), "c");

    auto [missing] = root.find_all("z");
    EXPECT_EQ(missing, nullptr);
}
} // namespace test
