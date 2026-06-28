#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/type.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestTypes: public ::testing::Test
{
    protected:
        TestTypes() { sihd::util::LoggerManager::stream(); }

        virtual ~TestTypes() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestTypes, test_types_from_str)
{
    EXPECT_EQ(type::from_str("none"), TYPE_NONE);
    EXPECT_EQ(type::from_str("bool"), TYPE_BOOL);
    EXPECT_EQ(type::from_str("char"), TYPE_CHAR);
    EXPECT_EQ(type::from_str("byte"), TYPE_BYTE);
    EXPECT_EQ(type::from_str("ubyte"), TYPE_UBYTE);
    EXPECT_EQ(type::from_str("short"), TYPE_SHORT);
    EXPECT_EQ(type::from_str("ushort"), TYPE_USHORT);
    EXPECT_EQ(type::from_str("int"), TYPE_INT);
    EXPECT_EQ(type::from_str("uint"), TYPE_UINT);
    EXPECT_EQ(type::from_str("long"), TYPE_LONG);
    EXPECT_EQ(type::from_str("ulong"), TYPE_ULONG);
    EXPECT_EQ(type::from_str("float"), TYPE_FLOAT);
    EXPECT_EQ(type::from_str("double"), TYPE_DOUBLE);
    EXPECT_EQ(type::from_str("object"), TYPE_OBJECT);
}

TEST_F(TestTypes, test_types_str)
{
    EXPECT_STREQ(type::str(TYPE_NONE), "none");
    EXPECT_STREQ(type::str(TYPE_BOOL), "bool");
    EXPECT_STREQ(type::str(TYPE_CHAR), "char");
    EXPECT_STREQ(type::str(TYPE_BYTE), "byte");
    EXPECT_STREQ(type::str(TYPE_UBYTE), "ubyte");
    EXPECT_STREQ(type::str(TYPE_SHORT), "short");
    EXPECT_STREQ(type::str(TYPE_USHORT), "ushort");
    EXPECT_STREQ(type::str(TYPE_INT), "int");
    EXPECT_STREQ(type::str(TYPE_UINT), "uint");
    EXPECT_STREQ(type::str(TYPE_LONG), "long");
    EXPECT_STREQ(type::str(TYPE_ULONG), "ulong");
    EXPECT_STREQ(type::str(TYPE_FLOAT), "float");
    EXPECT_STREQ(type::str(TYPE_DOUBLE), "double");
    EXPECT_STREQ(type::str(TYPE_OBJECT), "object");
}

TEST_F(TestTypes, test_types_template)
{
    EXPECT_EQ(type::from<bool>(), TYPE_BOOL);
    EXPECT_EQ(type::from<char>(), TYPE_CHAR);
    EXPECT_EQ(type::from<int8_t>(), TYPE_BYTE);
    EXPECT_EQ(type::from<uint8_t>(), TYPE_UBYTE);
    EXPECT_EQ(type::from<int16_t>(), TYPE_SHORT);
    EXPECT_EQ(type::from<uint16_t>(), TYPE_USHORT);
    EXPECT_EQ(type::from<int32_t>(), TYPE_INT);
    EXPECT_EQ(type::from<uint32_t>(), TYPE_UINT);
    EXPECT_EQ(type::from<int64_t>(), TYPE_LONG);
    EXPECT_EQ(type::from<uint64_t>(), TYPE_ULONG);
    EXPECT_EQ(type::from<float>(), TYPE_FLOAT);
    EXPECT_EQ(type::from<double>(), TYPE_DOUBLE);
    struct randomstruct
    {
            int hello;
    };
    EXPECT_EQ(type::from<randomstruct>(), TYPE_OBJECT);
}

TEST_F(TestTypes, test_type_sizes)
{
    EXPECT_EQ(type::size(TYPE_BOOL), sizeof(bool));
    EXPECT_EQ(type::size(TYPE_CHAR), sizeof(char));
    EXPECT_EQ(type::size(TYPE_BYTE), sizeof(int8_t));
    EXPECT_EQ(type::size(TYPE_UBYTE), sizeof(uint8_t));
    EXPECT_EQ(type::size(TYPE_SHORT), sizeof(int16_t));
    EXPECT_EQ(type::size(TYPE_USHORT), sizeof(uint16_t));
    EXPECT_EQ(type::size(TYPE_INT), sizeof(int32_t));
    EXPECT_EQ(type::size(TYPE_UINT), sizeof(uint32_t));
    EXPECT_EQ(type::size(TYPE_LONG), sizeof(int64_t));
    EXPECT_EQ(type::size(TYPE_ULONG), sizeof(uint64_t));
    EXPECT_EQ(type::size(TYPE_FLOAT), sizeof(float));
    EXPECT_EQ(type::size(TYPE_DOUBLE), sizeof(double));
}
} // namespace test
