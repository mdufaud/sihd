#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Datatype.hpp>

namespace test
{
    SIHD_LOGGER;
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
        EXPECT_EQ(Datatype::string_to_datatype("none"), DNONE);
        EXPECT_EQ(Datatype::string_to_datatype("bool"), DBOOL);
        EXPECT_EQ(Datatype::string_to_datatype("char"), DCHAR);
        EXPECT_EQ(Datatype::string_to_datatype("byte"), DBYTE);
        EXPECT_EQ(Datatype::string_to_datatype("ubyte"), DUBYTE);
        EXPECT_EQ(Datatype::string_to_datatype("short"), DSHORT);
        EXPECT_EQ(Datatype::string_to_datatype("ushort"), DUSHORT);
        EXPECT_EQ(Datatype::string_to_datatype("int"), DINT);
        EXPECT_EQ(Datatype::string_to_datatype("uint"), DUINT);
        EXPECT_EQ(Datatype::string_to_datatype("long"), DLONG);
        EXPECT_EQ(Datatype::string_to_datatype("ulong"), DULONG);
        EXPECT_EQ(Datatype::string_to_datatype("float"), DFLOAT);
        EXPECT_EQ(Datatype::string_to_datatype("double"), DDOUBLE);
        EXPECT_EQ(Datatype::string_to_datatype("string"), DSTRING);
        EXPECT_EQ(Datatype::string_to_datatype("object"), DOBJECT);
    }

    TEST_F(TestDatatype, test_datatype_str)
    {
        EXPECT_EQ(Datatype::datatype_to_string(DNONE), "none");
        EXPECT_EQ(Datatype::datatype_to_string(DBOOL), "bool");
        EXPECT_EQ(Datatype::datatype_to_string(DCHAR), "char");
        EXPECT_EQ(Datatype::datatype_to_string(DBYTE), "byte");
        EXPECT_EQ(Datatype::datatype_to_string(DUBYTE), "ubyte");
        EXPECT_EQ(Datatype::datatype_to_string(DSHORT), "short");
        EXPECT_EQ(Datatype::datatype_to_string(DUSHORT), "ushort");
        EXPECT_EQ(Datatype::datatype_to_string(DINT), "int");
        EXPECT_EQ(Datatype::datatype_to_string(DUINT), "uint");
        EXPECT_EQ(Datatype::datatype_to_string(DLONG), "long");
        EXPECT_EQ(Datatype::datatype_to_string(DULONG), "ulong");
        EXPECT_EQ(Datatype::datatype_to_string(DFLOAT), "float");
        EXPECT_EQ(Datatype::datatype_to_string(DDOUBLE), "double");
        EXPECT_EQ(Datatype::datatype_to_string(DSTRING), "string");
        EXPECT_EQ(Datatype::datatype_to_string(DOBJECT), "object");
    }

    TEST_F(TestDatatype, test_datatype_template)
    {
        EXPECT_EQ(Datatype::type_to_datatype<bool>(), DBOOL);
        EXPECT_EQ(Datatype::type_to_datatype<char>(), DCHAR);
        EXPECT_EQ(Datatype::type_to_datatype<int8_t>(), DBYTE);
        EXPECT_EQ(Datatype::type_to_datatype<uint8_t>(), DUBYTE);
        EXPECT_EQ(Datatype::type_to_datatype<int16_t>(), DSHORT);
        EXPECT_EQ(Datatype::type_to_datatype<uint16_t>(), DUSHORT);
        EXPECT_EQ(Datatype::type_to_datatype<int32_t>(), DINT);
        EXPECT_EQ(Datatype::type_to_datatype<uint32_t>(), DUINT);
        EXPECT_EQ(Datatype::type_to_datatype<int64_t>(), DLONG);
        EXPECT_EQ(Datatype::type_to_datatype<uint64_t>(), DULONG);
        EXPECT_EQ(Datatype::type_to_datatype<float>(), DFLOAT);
        EXPECT_EQ(Datatype::type_to_datatype<double>(), DDOUBLE);
        EXPECT_EQ(Datatype::type_to_datatype<std::string>(), DSTRING);
        struct randomstruct
        {
            int hello;
        };
        EXPECT_EQ(Datatype::type_to_datatype<randomstruct>(), DOBJECT);
    }

    TEST_F(TestDatatype, test_datatype_sizes)
    {
        EXPECT_EQ(Datatype::datatype_size(DBOOL), sizeof(bool));
        EXPECT_EQ(Datatype::datatype_size(DCHAR), sizeof(char));
        EXPECT_EQ(Datatype::datatype_size(DBYTE), sizeof(int8_t));
        EXPECT_EQ(Datatype::datatype_size(DUBYTE), sizeof(uint8_t));
        EXPECT_EQ(Datatype::datatype_size(DSHORT), sizeof(int16_t));
        EXPECT_EQ(Datatype::datatype_size(DUSHORT), sizeof(uint16_t));
        EXPECT_EQ(Datatype::datatype_size(DINT), sizeof(int32_t));
        EXPECT_EQ(Datatype::datatype_size(DUINT), sizeof(uint32_t));
        EXPECT_EQ(Datatype::datatype_size(DLONG), sizeof(int64_t));
        EXPECT_EQ(Datatype::datatype_size(DULONG), sizeof(uint64_t));
        EXPECT_EQ(Datatype::datatype_size(DFLOAT), sizeof(float));
        EXPECT_EQ(Datatype::datatype_size(DDOUBLE), sizeof(double));
    }
}
