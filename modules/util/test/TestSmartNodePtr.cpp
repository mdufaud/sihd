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
        // ptr goes out of scope → not owned by parent → SmartNodeDeleter calls delete
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
        // ptr goes out of scope → is_owned_by_parent() == true → SmartNodeDeleter skips delete
    }

    // child must still be accessible via parent
    EXPECT_EQ(parent.get_child("child"), child);
    EXPECT_FALSE(destroyed);
}

} // namespace test
