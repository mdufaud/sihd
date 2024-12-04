#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Types.hpp>

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
    EXPECT_EQ(Types::from_str("none"), TYPE_NONE);
    EXPECT_EQ(Types::from_str("bool"), TYPE_BOOL);
    EXPECT_EQ(Types::from_str("char"), TYPE_CHAR);
    EXPECT_EQ(Types::from_str("byte"), TYPE_BYTE);
    EXPECT_EQ(Types::from_str("ubyte"), TYPE_UBYTE);
    EXPECT_EQ(Types::from_str("short"), TYPE_SHORT);
    EXPECT_EQ(Types::from_str("ushort"), TYPE_USHORT);
    EXPECT_EQ(Types::from_str("int"), TYPE_INT);
    EXPECT_EQ(Types::from_str("uint"), TYPE_UINT);
    EXPECT_EQ(Types::from_str("long"), TYPE_LONG);
    EXPECT_EQ(Types::from_str("ulong"), TYPE_ULONG);
    EXPECT_EQ(Types::from_str("float"), TYPE_FLOAT);
    EXPECT_EQ(Types::from_str("double"), TYPE_DOUBLE);
    EXPECT_EQ(Types::from_str("object"), TYPE_OBJECT);
}

TEST_F(TestTypes, test_types_str)
{
    EXPECT_STREQ(Types::type_str(TYPE_NONE), "none");
    EXPECT_STREQ(Types::type_str(TYPE_BOOL), "bool");
    EXPECT_STREQ(Types::type_str(TYPE_CHAR), "char");
    EXPECT_STREQ(Types::type_str(TYPE_BYTE), "byte");
    EXPECT_STREQ(Types::type_str(TYPE_UBYTE), "ubyte");
    EXPECT_STREQ(Types::type_str(TYPE_SHORT), "short");
    EXPECT_STREQ(Types::type_str(TYPE_USHORT), "ushort");
    EXPECT_STREQ(Types::type_str(TYPE_INT), "int");
    EXPECT_STREQ(Types::type_str(TYPE_UINT), "uint");
    EXPECT_STREQ(Types::type_str(TYPE_LONG), "long");
    EXPECT_STREQ(Types::type_str(TYPE_ULONG), "ulong");
    EXPECT_STREQ(Types::type_str(TYPE_FLOAT), "float");
    EXPECT_STREQ(Types::type_str(TYPE_DOUBLE), "double");
    EXPECT_STREQ(Types::type_str(TYPE_OBJECT), "object");
}

TEST_F(TestTypes, test_types_template)
{
    EXPECT_EQ(Types::type<bool>(), TYPE_BOOL);
    EXPECT_EQ(Types::type<char>(), TYPE_CHAR);
    EXPECT_EQ(Types::type<int8_t>(), TYPE_BYTE);
    EXPECT_EQ(Types::type<uint8_t>(), TYPE_UBYTE);
    EXPECT_EQ(Types::type<int16_t>(), TYPE_SHORT);
    EXPECT_EQ(Types::type<uint16_t>(), TYPE_USHORT);
    EXPECT_EQ(Types::type<int32_t>(), TYPE_INT);
    EXPECT_EQ(Types::type<uint32_t>(), TYPE_UINT);
    EXPECT_EQ(Types::type<int64_t>(), TYPE_LONG);
    EXPECT_EQ(Types::type<uint64_t>(), TYPE_ULONG);
    EXPECT_EQ(Types::type<float>(), TYPE_FLOAT);
    EXPECT_EQ(Types::type<double>(), TYPE_DOUBLE);
    struct randomstruct
    {
            int hello;
    };
    EXPECT_EQ(Types::type<randomstruct>(), TYPE_OBJECT);
}

TEST_F(TestTypes, test_type_sizes)
{
    EXPECT_EQ(Types::type_size(TYPE_BOOL), sizeof(bool));
    EXPECT_EQ(Types::type_size(TYPE_CHAR), sizeof(char));
    EXPECT_EQ(Types::type_size(TYPE_BYTE), sizeof(int8_t));
    EXPECT_EQ(Types::type_size(TYPE_UBYTE), sizeof(uint8_t));
    EXPECT_EQ(Types::type_size(TYPE_SHORT), sizeof(int16_t));
    EXPECT_EQ(Types::type_size(TYPE_USHORT), sizeof(uint16_t));
    EXPECT_EQ(Types::type_size(TYPE_INT), sizeof(int32_t));
    EXPECT_EQ(Types::type_size(TYPE_UINT), sizeof(uint32_t));
    EXPECT_EQ(Types::type_size(TYPE_LONG), sizeof(int64_t));
    EXPECT_EQ(Types::type_size(TYPE_ULONG), sizeof(uint64_t));
    EXPECT_EQ(Types::type_size(TYPE_FLOAT), sizeof(float));
    EXPECT_EQ(Types::type_size(TYPE_DOUBLE), sizeof(double));
}
} // namespace test
