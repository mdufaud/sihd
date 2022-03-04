#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Types.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    class TestTypes:   public ::testing::Test
    {
        protected:
            TestTypes()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestTypes()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestTypes, test_types_from_str)
    {
        EXPECT_EQ(Types::string_to_type("none"), TYPE_NONE);
        EXPECT_EQ(Types::string_to_type("bool"), TYPE_BOOL);
        EXPECT_EQ(Types::string_to_type("char"), TYPE_CHAR);
        EXPECT_EQ(Types::string_to_type("byte"), TYPE_BYTE);
        EXPECT_EQ(Types::string_to_type("ubyte"), TYPE_UBYTE);
        EXPECT_EQ(Types::string_to_type("short"), TYPE_SHORT);
        EXPECT_EQ(Types::string_to_type("ushort"), TYPE_USHORT);
        EXPECT_EQ(Types::string_to_type("int"), TYPE_INT);
        EXPECT_EQ(Types::string_to_type("uint"), TYPE_UINT);
        EXPECT_EQ(Types::string_to_type("long"), TYPE_LONG);
        EXPECT_EQ(Types::string_to_type("ulong"), TYPE_ULONG);
        EXPECT_EQ(Types::string_to_type("float"), TYPE_FLOAT);
        EXPECT_EQ(Types::string_to_type("double"), TYPE_DOUBLE);
        EXPECT_EQ(Types::string_to_type("string"), TYPE_STRING);
        EXPECT_EQ(Types::string_to_type("object"), TYPE_OBJECT);
    }

    TEST_F(TestTypes, test_types_str)
    {
        EXPECT_EQ(Types::type_to_string(TYPE_NONE), "none");
        EXPECT_EQ(Types::type_to_string(TYPE_BOOL), "bool");
        EXPECT_EQ(Types::type_to_string(TYPE_CHAR), "char");
        EXPECT_EQ(Types::type_to_string(TYPE_BYTE), "byte");
        EXPECT_EQ(Types::type_to_string(TYPE_UBYTE), "ubyte");
        EXPECT_EQ(Types::type_to_string(TYPE_SHORT), "short");
        EXPECT_EQ(Types::type_to_string(TYPE_USHORT), "ushort");
        EXPECT_EQ(Types::type_to_string(TYPE_INT), "int");
        EXPECT_EQ(Types::type_to_string(TYPE_UINT), "uint");
        EXPECT_EQ(Types::type_to_string(TYPE_LONG), "long");
        EXPECT_EQ(Types::type_to_string(TYPE_ULONG), "ulong");
        EXPECT_EQ(Types::type_to_string(TYPE_FLOAT), "float");
        EXPECT_EQ(Types::type_to_string(TYPE_DOUBLE), "double");
        EXPECT_EQ(Types::type_to_string(TYPE_STRING), "string");
        EXPECT_EQ(Types::type_to_string(TYPE_OBJECT), "object");
    }

    TEST_F(TestTypes, test_types_template)
    {
        EXPECT_EQ(Types::to_type<bool>(), TYPE_BOOL);
        EXPECT_EQ(Types::to_type<char>(), TYPE_CHAR);
        EXPECT_EQ(Types::to_type<int8_t>(), TYPE_BYTE);
        EXPECT_EQ(Types::to_type<uint8_t>(), TYPE_UBYTE);
        EXPECT_EQ(Types::to_type<int16_t>(), TYPE_SHORT);
        EXPECT_EQ(Types::to_type<uint16_t>(), TYPE_USHORT);
        EXPECT_EQ(Types::to_type<int32_t>(), TYPE_INT);
        EXPECT_EQ(Types::to_type<uint32_t>(), TYPE_UINT);
        EXPECT_EQ(Types::to_type<int64_t>(), TYPE_LONG);
        EXPECT_EQ(Types::to_type<uint64_t>(), TYPE_ULONG);
        EXPECT_EQ(Types::to_type<float>(), TYPE_FLOAT);
        EXPECT_EQ(Types::to_type<double>(), TYPE_DOUBLE);
        EXPECT_EQ(Types::to_type<std::string>(), TYPE_STRING);
        struct randomstruct
        {
            int hello;
        };
        EXPECT_EQ(Types::to_type<randomstruct>(), TYPE_OBJECT);
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
}
