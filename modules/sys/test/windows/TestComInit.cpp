#include <objbase.h>

#include <thread>

#include <gtest/gtest.h>

#include <sihd/sys/windows/ComInit.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys::windows_;

class TestComInit: public ::testing::Test
{
    protected:
        TestComInit() { LoggerManager::stream(); }

        virtual ~TestComInit() { LoggerManager::clear_loggers(); }
};

TEST_F(TestComInit, test_multithreaded)
{
    HRESULT result = E_FAIL;
    APTTYPE type = APTTYPE_CURRENT;
    APTTYPEQUALIFIER qualifier = APTTYPEQUALIFIER_NONE;

    std::jthread([&] {
        ComInit com(ComInit::Threading::multithreaded);
        result = CoGetApartmentType(&type, &qualifier);
    }).join();

    EXPECT_EQ(result, S_OK);
    EXPECT_EQ(type, APTTYPE_MTA);
    EXPECT_EQ(qualifier, APTTYPEQUALIFIER_NONE);
}

TEST_F(TestComInit, test_other_threading_model_keeps_apartment)
{
    HRESULT result = E_FAIL;
    APTTYPE type = APTTYPE_CURRENT;
    APTTYPEQUALIFIER qualifier = APTTYPEQUALIFIER_NONE;

    std::jthread([&] {
        ComInit multithreaded(ComInit::Threading::multithreaded);
        {
            ComInit apartment(ComInit::Threading::apartment);
        }
        result = CoGetApartmentType(&type, &qualifier);
    }).join();

    EXPECT_EQ(result, S_OK);
    EXPECT_EQ(type, APTTYPE_MTA);
}

TEST_F(TestComInit, test_nested_scopes_keep_apartment)
{
    HRESULT result = E_FAIL;
    APTTYPE type = APTTYPE_CURRENT;
    APTTYPEQUALIFIER qualifier = APTTYPEQUALIFIER_NONE;

    std::jthread([&] {
        ComInit outer(ComInit::Threading::multithreaded);
        {
            ComInit inner(ComInit::Threading::multithreaded);
        }
        result = CoGetApartmentType(&type, &qualifier);
    }).join();

    EXPECT_EQ(result, S_OK);
    EXPECT_EQ(type, APTTYPE_MTA);
}

} // namespace test
