#include <utility>

#include <gtest/gtest.h>

#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

namespace test
{
using namespace sihd::util;

// gcc-16 -O3 speculation inlines ~TrackNamed into a base-typed deleter and
// false-positives -Warray-bounds: both tests must allocate TrackNamed
class TrackNamed: public Named
{
    public:
        bool & _destroyed;
        TrackNamed(const std::string & name, bool & d): Named(name), _destroyed(d) {}
        ~TrackNamed() { _destroyed = true; }
};

class TestSmartNodePtr: public ::testing::Test
{
    protected:
        TestSmartNodePtr() = default;
        virtual ~TestSmartNodePtr() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestSmartNodePtr, test_smart_node_ptr_deletes_unowned)
{
    bool destroyed = false;

    {
        SmartNodePtr<TrackNamed> ptr(new TrackNamed("tracked", destroyed));
        EXPECT_FALSE(destroyed);
    }
    EXPECT_TRUE(destroyed);
}

TEST_F(TestSmartNodePtr, test_smart_node_ptr_skips_owned)
{
    bool destroyed = false;
    Node parent("parent");
    TrackNamed *child = new TrackNamed("child", destroyed);
    ASSERT_TRUE(parent.add_child(child, true)); // parent takes ownership

    EXPECT_TRUE(child->is_owned_by_parent());

    {
        SmartNodePtr<TrackNamed> ptr(child);
    }

    EXPECT_EQ(parent.get_child("child"), child);
    EXPECT_FALSE(destroyed);
}

TEST_F(TestSmartNodePtr, test_smart_node_ptr_outlives_a_node_its_parent_deleted)
{
    bool destroyed = false;
    Node parent("parent");
    TrackNamed *child = new TrackNamed("child", destroyed);
    ASSERT_TRUE(parent.add_child(child, true));

    {
        SmartNodePtr<TrackNamed> ptr(child);
        parent.remove_children();
        EXPECT_TRUE(destroyed);
    }

    EXPECT_EQ(parent.get_child("child"), nullptr);
}

TEST_F(TestSmartNodePtr, test_smart_node_ptr_holds_nullptr)
{
    SmartNodePtr<TrackNamed> ptr(nullptr);

    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.get(), nullptr);
}

TEST_F(TestSmartNodePtr, test_smart_node_ptr_move_keeps_one_owner)
{
    bool destroyed = false;
    SmartNodePtr<TrackNamed> first(new TrackNamed("tracked", destroyed));
    TrackNamed *node = first.get();

    SmartNodePtr<TrackNamed> second(std::move(first));

    EXPECT_EQ(first.get(), nullptr);
    EXPECT_EQ(second.get(), node);
    second = SmartNodePtr<TrackNamed>();
    EXPECT_TRUE(destroyed);
}

TEST_F(TestSmartNodePtr, test_smart_node_ptr_move_assign_deletes_the_held_node)
{
    bool replaced_destroyed = false;
    bool kept_destroyed = false;
    SmartNodePtr<TrackNamed> held(new TrackNamed("replaced", replaced_destroyed));
    SmartNodePtr<TrackNamed> other(new TrackNamed("kept", kept_destroyed));

    held = std::move(other);

    EXPECT_TRUE(replaced_destroyed);
    EXPECT_FALSE(kept_destroyed);
    EXPECT_EQ(held->name(), "kept");
}

} // namespace test
