#include <gtest/gtest.h>

#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

namespace test
{
using namespace sihd::util;

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

    struct TrackNamed: public Named
    {
            bool & _destroyed;
            TrackNamed(bool & d): Named("tracked"), _destroyed(d) {}
            ~TrackNamed() { _destroyed = true; }
    };

    {
        SmartNodePtr<TrackNamed> ptr(new TrackNamed(destroyed));
        EXPECT_FALSE(destroyed);
        // ptr goes out of scope → not owned by parent → SmartNodeDeleter calls delete
    }
    EXPECT_TRUE(destroyed);
}

TEST_F(TestSmartNodePtr, test_smart_node_ptr_skips_owned)
{
    Node parent("parent");
    Named *child = new Named("child");
    parent.add_child(child, true); // parent takes ownership

    EXPECT_TRUE(child->is_owned_by_parent());

    {
        SmartNodePtr<Named> ptr(child);
        // ptr goes out of scope → is_owned_by_parent() == true → SmartNodeDeleter skips delete
    }

    // child must still be accessible via parent
    EXPECT_EQ(parent.get_child("child"), child);
}

} // namespace test
