#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Configurable.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;


	class ConfigurableObj:	public Configurable
	{
		public:
			ConfigurableObj()
			{
				// By lambda
				this->add_conf<bool>("bool", [&] (bool test) { this->bool_val = test; return true; });
				// By bind
				this->add_conf<int8_t>("char", std::bind(&ConfigurableObj::set_char, this, std::placeholders::_1));
				// By ptr
				this->add_conf("uchar", &ConfigurableObj::set_uchar);
				this->add_conf("short", &ConfigurableObj::set_short);
				this->add_conf("ushort", &ConfigurableObj::set_ushort);
				this->add_conf("int", &ConfigurableObj::set_int);
				this->add_conf("uint", &ConfigurableObj::set_uint);
				this->add_conf("long", &ConfigurableObj::set_long);
				this->add_conf("ulong", &ConfigurableObj::set_ulong);
				this->add_conf("float", &ConfigurableObj::set_float);
				this->add_conf("double", &ConfigurableObj::set_double);
				this->add_conf("str", &ConfigurableObj::set_str);
				this->add_conf("cstr", &ConfigurableObj::set_cstr);
				// Multiple args
				this->add_conf<bool, int>("dual", [&] (bool b, int i) { TRACE(b << " " << i); return true; });
			}

			~ConfigurableObj() {};

			bool	set_bool(bool test)
			{
				bool_val = test;
				return true;
			}

			bool	set_char(int8_t test)
			{
				char_val = test;
				return true;
			}

			bool	set_uchar(uint8_t test)
			{
				uchar_val = test;
				return true;
			}

			bool	set_short(int16_t test)
			{
				short_val = test;
				return true;
			}

			bool	set_ushort(uint16_t test)
			{
				ushort_val = test;
				return true;
			}

			bool	set_int(int32_t test)
			{
				int_val = test;
				return true;
			}

			bool	set_uint(uint32_t test)
			{
				uint_val = test;
				return true;
			}

			bool	set_long(int64_t test)
			{
				long_val = test;
				return true;
			}

			bool	set_ulong(uint64_t test)
			{
				ulong_val = test;
				return true;
			}

			bool	set_float(float test)
			{
				float_val = test;
				return true;
			}

			bool	set_double(double test)
			{
				double_val = test;
				return true;
			}

			bool	set_cstr(const char *str)
			{
				str_val = str;
				return true;
			}

			bool	set_str(std::string str)
			{
				str_val = str;
				return true;
			}

			bool	bool_val = false;
			int8_t		char_val = 0;
			uint8_t		uchar_val = 0;
			int16_t		short_val = 0;
			uint16_t	ushort_val = 0;
			int32_t		int_val = 0;
			uint32_t	uint_val = 0;
			int64_t 	long_val = 0;
			uint64_t	ulong_val = 0;
			float	float_val = 0.0; 
			double	double_val = 0.0;
			std::string str_val = "";
	};

    class TestConfigurable:   public ::testing::Test
    {
        protected:
            TestConfigurable()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestConfigurable()
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

    TEST_F(TestConfigurable, test_configurable_callback)
	{
		CallbackManager cbm;
		cbm.set<bool>("test_noarg", [] () -> bool { return true; });
		EXPECT_EQ(cbm.call<bool>("test_noarg"), true);
		cbm.set<bool, int>("test_1arg", [] (int i) -> bool { return i % 2 == 0; });
		EXPECT_EQ(cbm.call<bool>("test_1arg", 2), true);
		// gtest derping
		bool ret = cbm.call<bool, int>("test_1arg", 3);
		EXPECT_EQ(ret, false);
	}

    TEST_F(TestConfigurable, test_configurable_class)
    {
		ConfigurableObj obj;

		obj.set_conf("dual", false, 10);

		EXPECT_EQ(obj.bool_val, false);
		obj.set_conf("bool", true);
		EXPECT_EQ(obj.bool_val, true);

		// Default template is int
		EXPECT_EQ(obj.int_val, 0);
		obj.set_conf("int", 20);
		EXPECT_EQ(obj.int_val, 20);

		// Try catch for good type
		EXPECT_EQ(obj.char_val, 0);
		obj.set_conf_int("char", 'a');
		EXPECT_EQ(obj.char_val, 'a');

		EXPECT_EQ(obj.uchar_val, 0);
		obj.set_conf_int("uchar", -1);
		EXPECT_EQ(obj.uchar_val, 255);

		EXPECT_EQ(obj.short_val, 0);
		obj.set_conf_int("short", -32769);
		EXPECT_EQ(obj.short_val, 32767);

		EXPECT_EQ(obj.ushort_val, 0);
		obj.set_conf_int("ushort", -1);
		EXPECT_EQ(obj.ushort_val, 65535);

		EXPECT_EQ(obj.int_val, 20);
		obj.set_conf_int("int", -123);
		EXPECT_EQ(obj.int_val, -123);

		EXPECT_EQ(obj.uint_val, 0u);
		obj.set_conf_int("uint", -1);
		EXPECT_EQ(obj.uint_val, 4294967295);

		EXPECT_EQ(obj.long_val, 0l);
		obj.set_conf_int("long", __LONG_LONG_MAX__);
		EXPECT_EQ(obj.long_val, __LONG_LONG_MAX__);

		EXPECT_EQ(obj.ulong_val, 0ul);
		obj.set_conf_int("ulong", -1);
		EXPECT_EQ(obj.ulong_val, 18446744073709551615ul);

		// Floats

		// double is default type
		EXPECT_EQ(obj.double_val, 0.0);
		obj.set_conf("double", 11.11);
		EXPECT_FLOAT_EQ(obj.double_val, 11.11);

		EXPECT_EQ(obj.float_val, 0.0);
		obj.set_conf_float("float", 123.11);
		EXPECT_FLOAT_EQ(obj.float_val, 123.11);

		EXPECT_FLOAT_EQ(obj.double_val, 11.11);
		obj.set_conf_float("double", 12345.678);
		EXPECT_FLOAT_EQ(obj.double_val, 12345.678);

		// Strings

		// default is const char
		EXPECT_EQ(obj.str_val, "");
		obj.set_conf("cstr", "hello");
		EXPECT_EQ(obj.str_val, "hello");
		obj.set_conf<std::string>("str", "world");
		EXPECT_EQ(obj.str_val, "world");
		obj.set_conf_str("str", "hello world");
		EXPECT_EQ(obj.str_val, "hello world");
		obj.set_conf_str("cstr", "hello world sup");
		EXPECT_EQ(obj.str_val, "hello world sup");
    }
}
