#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <sihd/core/Node.hpp>

namespace test
{
    using namespace sihd::core;
    class TestNode:   public ::testing::Test
    {
        protected:
            TestNode()
            {}
            virtual ~TestNode()
            {}
            virtual void SetUp()
            {}
            virtual void TearDown()
            {}

            void build_tree()
            {

            }
    };

    TEST_F(TestNode, test_named)
    {

    }

    TEST_F(TestNode, test_tree)
    {
        Node *root = new Node("root");
        root->add_child(new Named("child1"));
        root->add_child(new Named("child2"));
        Named *child3 = new Named("child3", root);
        Node *parent = new Node("parent");
        root->add_child(parent);
        parent->add_child(new Named("cousin1"));
        parent->add_child(new Named("cousin2"));
        Named *cousin3 = new Named("cousin3", parent);

        root->print_tree();

        // Parent - child
        Named *child1 = root->find("child1");
        EXPECT_NE(child1, nullptr);
        EXPECT_EQ(child1->get_name(), "child1");
        EXPECT_EQ(root->get_child("child3"), child3);
        EXPECT_EQ(parent->get_parent(), root);

        // Node casting
        EXPECT_EQ(Node::to_node(root->get_child("parent")), parent);
        EXPECT_EQ(Node::to_node(child3), nullptr);

        // Find
        Named *found = root->find("parent.cousin1");
        EXPECT_NE(found, nullptr);
        EXPECT_EQ(found->get_name(), "cousin1");
        EXPECT_EQ(found, root->find(".parent.cousin1"));
        EXPECT_EQ(found, parent->find(".cousin1"));
        EXPECT_EQ(found, parent->find("..parent.cousin1"));
        EXPECT_EQ(root, child3->find(".."));

        // Root
        EXPECT_EQ(parent->get_root(), root);
        // From root
        EXPECT_EQ(cousin3, parent->find("parent.cousin3"));
        EXPECT_EQ(cousin3, root->find("parent.cousin3"));
    }
}
