#include <gtest/gtest.h>

#include <sihd/util/Collector.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/IProvider.hpp>

namespace test
{
using namespace sihd::util;

class IntProvider: public IProvider<int>
{
    public:
        IntProvider(int val): _val(val), _providing(true) {}

        bool provide(int *data) override
        {
            *data = _val;
            return true;
        }
        bool providing() const override { return _providing; }

        void set_providing(bool v) { _providing = v; }
        void set_value(int v) { _val = v; }

    private:
        int _val;
        bool _providing;
};

class TestCollector: public ::testing::Test
{
    protected:
        TestCollector() = default;
        virtual ~TestCollector() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestCollector, test_collector_collect)
{
    IntProvider provider(42);
    Collector<int> collector(&provider);

    EXPECT_TRUE(collector.can_collect());
    EXPECT_TRUE(collector.collect());
    EXPECT_EQ(collector.data(), 42);

    provider.set_value(100);
    EXPECT_TRUE(collector.collect());
    EXPECT_EQ(collector.data(), 100);
}

TEST_F(TestCollector, test_collector_not_providing)
{
    IntProvider provider(1);
    provider.set_providing(false);

    Collector<int> collector(&provider);
    EXPECT_FALSE(collector.can_collect());
    EXPECT_FALSE(collector.collect());
}

} // namespace test
