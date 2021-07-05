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
        EXPECT_EQ(Datatype::string_to_datatype("NONE"), NONE); 
        EXPECT_EQ(Datatype::string_to_datatype("BOOL"), BOOL); 
        EXPECT_EQ(Datatype::string_to_datatype("CHAR"), CHAR);
        EXPECT_EQ(Datatype::string_to_datatype("BYTE"), BYTE);
        EXPECT_EQ(Datatype::string_to_datatype("UBYTE"), UBYTE);
        EXPECT_EQ(Datatype::string_to_datatype("SHORT"), SHORT);
        EXPECT_EQ(Datatype::string_to_datatype("USHORT"), USHORT);
        EXPECT_EQ(Datatype::string_to_datatype("INT"), INT);
        EXPECT_EQ(Datatype::string_to_datatype("UINT"), UINT);
        EXPECT_EQ(Datatype::string_to_datatype("LONG"), LONG);
        EXPECT_EQ(Datatype::string_to_datatype("ULONG"), ULONG);
        EXPECT_EQ(Datatype::string_to_datatype("FLOAT"), FLOAT);
        EXPECT_EQ(Datatype::string_to_datatype("DOUBLE"), DOUBLE);
        EXPECT_EQ(Datatype::string_to_datatype("STRING"), STRING); 
        EXPECT_EQ(Datatype::string_to_datatype("OBJECT"), OBJECT); 
    }

    TEST_F(TestDatatype, test_datatype_str)
    {
        EXPECT_EQ(Datatype::datatype_to_string(NONE), "NONE"); 
        EXPECT_EQ(Datatype::datatype_to_string(BOOL), "BOOL"); 
        EXPECT_EQ(Datatype::datatype_to_string(CHAR), "CHAR");
        EXPECT_EQ(Datatype::datatype_to_string(BYTE), "BYTE");
        EXPECT_EQ(Datatype::datatype_to_string(UBYTE), "UBYTE");
        EXPECT_EQ(Datatype::datatype_to_string(SHORT), "SHORT");
        EXPECT_EQ(Datatype::datatype_to_string(USHORT), "USHORT");
        EXPECT_EQ(Datatype::datatype_to_string(INT), "INT");
        EXPECT_EQ(Datatype::datatype_to_string(UINT), "UINT");
        EXPECT_EQ(Datatype::datatype_to_string(LONG), "LONG");
        EXPECT_EQ(Datatype::datatype_to_string(ULONG), "ULONG");
        EXPECT_EQ(Datatype::datatype_to_string(FLOAT), "FLOAT");
        EXPECT_EQ(Datatype::datatype_to_string(DOUBLE), "DOUBLE");
        EXPECT_EQ(Datatype::datatype_to_string(STRING), "STRING"); 
        EXPECT_EQ(Datatype::datatype_to_string(OBJECT), "OBJECT"); 
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
