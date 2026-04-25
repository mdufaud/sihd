#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestNode: public ::testing::Test
{
    protected:
        TestNode() { sihd::util::LoggerManager::stream(); }

        virtual ~TestNode() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestNode, test_node_tree)
{
    constexpr bool get_pointer_ownership = true;

    Node root("root");
    root.add_child(new Named("child1"), get_pointer_ownership);
    root.add_child(new Named("child2"), get_pointer_ownership);
    Named *child3 = new Named("child3", &root);
    Node *parent = new Node("parent");
    root.add_child(parent, get_pointer_ownership);
    parent->add_child(new Named("cousin1"), get_pointer_ownership);
    parent->add_child(new Named("cousin2"), get_pointer_ownership);
    Named *cousin3 = parent->add_child<Named>("cousin3");

    Node *parent_found = nullptr;
    root.execute_children<Node>([&parent_found](Node *child) { parent_found = child; });
    EXPECT_EQ(parent_found, parent);

    SIHD_COUT("{}\n", root.tree_str());

    // Parent - child
    auto [child1, child2] = root.find_all("child1", "child2");

    EXPECT_NE(child2, nullptr);
    EXPECT_NE(root.find("child1"), nullptr);

    ASSERT_NE(child1, nullptr);

    EXPECT_EQ(child1->name(), "child1");
    EXPECT_EQ(root.get_child("child3"), child3);
    EXPECT_EQ(parent->parent(), &root);
    EXPECT_EQ(child1->root(), &root);

    // Node casting
    EXPECT_EQ(dynamic_cast<Node *>(root.get_child("parent")), parent);
    EXPECT_EQ(dynamic_cast<Node *>(child3), nullptr);

    // Find
    Named *found = root.find("parent.cousin1");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name(), "cousin1");
    EXPECT_EQ(found, parent->find("cousin1"));
    EXPECT_EQ(found, parent->find(".cousin1"));
    EXPECT_EQ(found, root.find(".parent.cousin1"));
    EXPECT_EQ(found, parent->find("..parent.cousin1"));
    EXPECT_EQ(&root, child3->find(".."));
    EXPECT_EQ(&root, child3->find<Node>(".."));

    // Root
    EXPECT_EQ(parent->root(), &root);
    // From root
    EXPECT_EQ(cousin3, parent->find("/parent.cousin3"));
    EXPECT_EQ(cousin3, root.find("parent.cousin3"));
}

TEST_F(TestNode, test_node_links)
{
    Node root("root");
    Node *origin = root.add_child<Node>("origin");
    Named *child1 = origin->add_child<Named>("child1");
    Named *child2 = origin->add_child<Named>("child2");
    Node *other_family = root.add_child<Node>("other_family");
    Node *older = root.add_child<Node>("older");

    Node *parent_node = root.add_child<Node>("parent");
    Node *uncle_node = other_family->add_child<Node>("uncle");
    Node *gp_node = older->add_child<Node>("grandparent");

    auto [tmp_origin, tmp_parent] = root.get_all_child<Node>("origin", "parent");
    EXPECT_EQ(tmp_origin, origin);
    EXPECT_EQ(tmp_parent, parent_node);

    auto [tmp_gp, tmp_uncle] = root.find_all<Node>("older.grandparent", "other_family.uncle");
    EXPECT_EQ(tmp_uncle, uncle_node);
    EXPECT_EQ(tmp_gp, gp_node);

    SIHD_COUT("{}\n", root.tree_str());

    parent_node->add_link("mychild1", "..origin.child1");
    uncle_node->add_link("mycousin1", "...parent.mychild1");
    uncle_node->add_link("mycousin2", "...origin.child2");
    gp_node->add_link("mygrandchild1", "...other_family.uncle.mycousin1");
    gp_node->add_link("mygrandchild2", "/other_family.uncle.mycousin2");

    EXPECT_TRUE(gp_node->resolve_links());

    SIHD_COUT("{}\n", root.tree_str());

    EXPECT_EQ(parent_node->get_child("mychild1"), child1);
    EXPECT_EQ(uncle_node->get_child("mycousin1"), child1);
    EXPECT_EQ(uncle_node->get_child("mycousin2"), child2);
    EXPECT_EQ(gp_node->get_child("mygrandchild1"), child1);
    EXPECT_EQ(gp_node->get_child("mygrandchild2"), child2);
}

TEST_F(TestNode, test_node_errors)
{
    Node root("root");
    new Node("parent", &root);
    EXPECT_THROW(Node("parent", &root), std::invalid_argument);
    Named *n1 = new Named("test");
    Named *n2 = new Named("test");
    EXPECT_TRUE(root.add_child(n1, true));
    EXPECT_FALSE(root.add_child(n2, false));
    EXPECT_TRUE(root.add_link("name", "..some.path"));
    EXPECT_FALSE(root.add_link("name", "..some.other.path"));
    delete n2;
}

TEST_F(TestNode, test_node_add_child_unsafe_throws)
{
    Node root("root");
    root.add_child(new Named("dup"), true);
    Named *dup2 = new Named("dup");
    EXPECT_THROW(root.add_child_unsafe(dup2, false), std::invalid_argument);
    delete dup2;
}

TEST_F(TestNode, test_node_ownership)
{
    Node root("root");
    Named *owned = new Named("owned");
    Named *borrowed = new Named("borrowed");

    root.add_child(owned, true);
    root.add_child(borrowed, false);

    EXPECT_TRUE(root.has_ownership(owned));
    EXPECT_TRUE(root.has_ownership("owned"));
    EXPECT_FALSE(root.has_ownership(borrowed));
    EXPECT_FALSE(root.has_ownership("borrowed"));

    root.set_child_ownership("borrowed", true);
    EXPECT_TRUE(root.has_ownership(borrowed));

    root.set_child_ownership(owned, false);
    EXPECT_FALSE(root.has_ownership(owned));
    // cleanup manually since ownership was released
    delete owned;
}

TEST_F(TestNode, test_node_remove_children)
{
    Node root("root");
    root.add_child(new Named("a"), true);
    root.add_child(new Named("b"), true);
    root.add_child(new Named("c"), true);

    EXPECT_NE(root.get_child("a"), nullptr);
    root.remove_children();
    EXPECT_EQ(root.get_child("a"), nullptr);
    EXPECT_EQ(root.get_child("b"), nullptr);
    EXPECT_EQ(root.get_child("c"), nullptr);
}

} // namespace test
