#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Datatype.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestDatatype:   public ::testing::Test
    {
        protected:
            TestDatatype()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestDatatype()
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

    TEST_F(TestDatatype, test_datatype_from_str)
    {
        EXPECT_EQ(Datatype::string_to_datatype("none"), NONE); 
        EXPECT_EQ(Datatype::string_to_datatype("bool"), BOOL); 
        EXPECT_EQ(Datatype::string_to_datatype("char"), CHAR);
        EXPECT_EQ(Datatype::string_to_datatype("byte"), BYTE);
        EXPECT_EQ(Datatype::string_to_datatype("ubyte"), UBYTE);
        EXPECT_EQ(Datatype::string_to_datatype("short"), SHORT);
        EXPECT_EQ(Datatype::string_to_datatype("ushort"), USHORT);
        EXPECT_EQ(Datatype::string_to_datatype("int"), INT);
        EXPECT_EQ(Datatype::string_to_datatype("uint"), UINT);
        EXPECT_EQ(Datatype::string_to_datatype("long"), LONG);
        EXPECT_EQ(Datatype::string_to_datatype("ulong"), ULONG);
        EXPECT_EQ(Datatype::string_to_datatype("float"), FLOAT);
        EXPECT_EQ(Datatype::string_to_datatype("double"), DOUBLE);
        EXPECT_EQ(Datatype::string_to_datatype("string"), STRING); 
        EXPECT_EQ(Datatype::string_to_datatype("object"), OBJECT); 
    }

    TEST_F(TestDatatype, test_datatype_str)
    {
        EXPECT_EQ(Datatype::datatype_to_string(NONE), "none"); 
        EXPECT_EQ(Datatype::datatype_to_string(BOOL), "bool"); 
        EXPECT_EQ(Datatype::datatype_to_string(CHAR), "char");
        EXPECT_EQ(Datatype::datatype_to_string(BYTE), "byte");
        EXPECT_EQ(Datatype::datatype_to_string(UBYTE), "ubyte");
        EXPECT_EQ(Datatype::datatype_to_string(SHORT), "short");
        EXPECT_EQ(Datatype::datatype_to_string(USHORT), "ushort");
        EXPECT_EQ(Datatype::datatype_to_string(INT), "int");
        EXPECT_EQ(Datatype::datatype_to_string(UINT), "uint");
        EXPECT_EQ(Datatype::datatype_to_string(LONG), "long");
        EXPECT_EQ(Datatype::datatype_to_string(ULONG), "ulong");
        EXPECT_EQ(Datatype::datatype_to_string(FLOAT), "float");
        EXPECT_EQ(Datatype::datatype_to_string(DOUBLE), "double");
        EXPECT_EQ(Datatype::datatype_to_string(STRING), "string"); 
        EXPECT_EQ(Datatype::datatype_to_string(OBJECT), "object"); 
    }

    TEST_F(TestDatatype, test_datatype_template)
    {
        EXPECT_EQ(Datatype::type_to_datatype<bool>(), BOOL);
        EXPECT_EQ(Datatype::type_to_datatype<char>(), CHAR);
        EXPECT_EQ(Datatype::type_to_datatype<int8_t>(), BYTE);
        EXPECT_EQ(Datatype::type_to_datatype<uint8_t>(), UBYTE);
        EXPECT_EQ(Datatype::type_to_datatype<int16_t>(), SHORT);
        EXPECT_EQ(Datatype::type_to_datatype<uint16_t>(), USHORT);
        EXPECT_EQ(Datatype::type_to_datatype<int32_t>(), INT);
        EXPECT_EQ(Datatype::type_to_datatype<uint32_t>(), UINT);
        EXPECT_EQ(Datatype::type_to_datatype<int64_t>(), LONG);
        EXPECT_EQ(Datatype::type_to_datatype<uint64_t>(), ULONG);
        EXPECT_EQ(Datatype::type_to_datatype<float>(), FLOAT);
        EXPECT_EQ(Datatype::type_to_datatype<double>(), DOUBLE);
        EXPECT_EQ(Datatype::type_to_datatype<std::string>(), STRING);
        struct randomstruct
        {
            int hello;
        };
        EXPECT_EQ(Datatype::type_to_datatype<randomstruct>(), OBJECT);
    }

    TEST_F(TestDatatype, test_datatype_sizes)
    {
        EXPECT_EQ(Datatype::datatype_size(BOOL), sizeof(bool));
        EXPECT_EQ(Datatype::datatype_size(CHAR), sizeof(char));
        EXPECT_EQ(Datatype::datatype_size(BYTE), sizeof(int8_t));
        EXPECT_EQ(Datatype::datatype_size(UBYTE), sizeof(uint8_t));
        EXPECT_EQ(Datatype::datatype_size(SHORT), sizeof(int16_t));
        EXPECT_EQ(Datatype::datatype_size(USHORT), sizeof(uint16_t));
        EXPECT_EQ(Datatype::datatype_size(INT), sizeof(int32_t));
        EXPECT_EQ(Datatype::datatype_size(UINT), sizeof(uint32_t));
        EXPECT_EQ(Datatype::datatype_size(LONG), sizeof(int64_t));
        EXPECT_EQ(Datatype::datatype_size(ULONG), sizeof(uint64_t));
        EXPECT_EQ(Datatype::datatype_size(FLOAT), sizeof(float));
        EXPECT_EQ(Datatype::datatype_size(DOUBLE), sizeof(double));
    }
}
