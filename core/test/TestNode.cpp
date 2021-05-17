#include <gtest/gtest.h>
#include <iostream>
#include <sihd/core/Logger.hpp>
#include <sihd/core/Node.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::core;
    class TestNode:   public ::testing::Test
    {
        protected:
            TestNode()
            {
                sihd::core::LoggerManager::basic();
                LOG(info, "LOL")
            }

            virtual ~TestNode()
            {
                sihd::core::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestNode, test_named_core)
    {
        Named named("obj");
        EXPECT_EQ(named.get_name(), "obj");
        EXPECT_EQ(named.get_parent(), nullptr);
    }
    
    TEST_F(TestNode, test_named_find)
    {
        Named named("obj");
        EXPECT_EQ(named.find("."), &named);
        EXPECT_EQ(named.find_node("."), nullptr);
        EXPECT_EQ(named.find(".."), nullptr);
        EXPECT_EQ(named.find("/"), nullptr);
    }

    TEST_F(TestNode, test_node_tree)
    {
        Node root("root");
        root.add_child(new Named("child1"));
        root.add_child(new Named("child2"));
        Named *child3 = new Named("child3", &root);
        Node *parent = new Node("parent");
        root.add_child(parent);
        parent->add_child(new Named("cousin1"));
        parent->add_child(new Named("cousin2"));
        Named *cousin3 = new Named("cousin3", parent);

        root.print_tree();

        // Parent - child
        Named *child1 = root.find("child1");
        EXPECT_NE(child1, nullptr);
        EXPECT_EQ(child1->get_name(), "child1");
        EXPECT_EQ(root.get_child("child3"), child3);
        EXPECT_EQ(parent->get_parent(), &root);

        // Node casting
        EXPECT_EQ(Node::to_node(root.get_child("parent")), parent);
        EXPECT_EQ(Node::to_node(child3), nullptr);

        // Find
        Named *found = root.find("parent.cousin1");
        EXPECT_NE(found, nullptr);
        EXPECT_EQ(found->get_name(), "cousin1");
        EXPECT_EQ(found, root.find(".parent.cousin1"));
        EXPECT_EQ(found, parent->find(".cousin1"));
        EXPECT_EQ(found, parent->find("..parent.cousin1"));
        EXPECT_EQ(&root, child3->find(".."));
        EXPECT_EQ(&root, child3->find_node(".."));

        // Root
        EXPECT_EQ(parent->get_root(), &root);
        // From root
        EXPECT_EQ(cousin3, parent->find("parent.cousin3"));
        EXPECT_EQ(cousin3, root.find("parent.cousin3"));
    }

    TEST_F(TestNode, test_node_links)
    {
        Node root("root");
        Node *origin = new Node("origin", &root);
        Named *child1 = new Named("child1", origin);
        Named *child2 = new Named("child2", origin);
        Node *other_family = new Node("other_family", &root);
        Node *older = new Node("older", &root);

        Node *parent_node = new Node("parent", &root);
        Node *uncle_node = new Node("uncle", other_family);
        Node *gp_node = new Node("grandparent", older);

        root.print_tree();

        parent_node->add_link("mychild1", "..origin.child1");
        uncle_node->add_link("mycousin1", "...parent.mychild1");
        uncle_node->add_link("mycousin2", "...origin.child2");
        gp_node->add_link("mygrandchild1", "...other_family.uncle.mycousin1");
        gp_node->add_link("mygrandchild2", "other_family.uncle.mycousin2");

        EXPECT_TRUE(gp_node->resolve_links());

        root.print_tree();

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
        EXPECT_THROW(new Node("parent", &root), Node::AlreadyHasChild);
        Named *n1 = new Named("test");
        Named *n2 = new Named("test");
        EXPECT_TRUE(root.add_child(n1));
        EXPECT_FALSE(root.add_child(n2));
        EXPECT_TRUE(root.add_link("name", "..some.path"));
        EXPECT_FALSE(root.add_link("name", "..some.other.path"));
        delete n2;
    }

}
