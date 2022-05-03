#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/zip/ZipWriter.hpp>
#include <sihd/zip/ZipReader.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/Array.hpp>

namespace test
{
    SIHD_NEW_LOGGER("test");
    using namespace sihd::zip;
    using namespace sihd::util;
    class TestZip:   public ::testing::Test
    {
        protected:
            TestZip()
            {
                sihd::util::LoggerManager::basic();
                _test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::Files::combine(_test_path, "zip");
            }

            virtual ~TestZip()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
                sihd::util::Files::make_directory(_base_test_dir);
            }

            virtual void TearDown()
            {
            }

            std::string _base_test_dir;
            std::string _test_path;
    };

    TEST_F(TestZip, test_zip_writer)
    {
        std::string zip_path = Files::combine(_base_test_dir, "to_zip.zip");
        Files::remove_file(zip_path);

        ZipWriter writer("zip-writer");

        EXPECT_TRUE(writer.set_aes_encryption(128));
        EXPECT_FALSE(writer.set_conf("aes", 10));

        SIHD_LOG(info, "Creating zip archive: " << zip_path);
        EXPECT_TRUE(writer.open(zip_path));

        // Adding fs directory
        std::string entry = "test/resources/to_zip";
        SIHD_LOG(info, "Zipping: " << entry);
        EXPECT_TRUE(writer.fs_add(entry, "to_zip"));

        // Adding array entry
        sihd::util::ArrChar arr_entry("hello test world");
        EXPECT_TRUE(writer.add_file("toto_entry", arr_entry.cbuf(), arr_entry.byte_size()));
        EXPECT_TRUE(writer.add_file("toto_entry2", arr_entry.cbuf(), arr_entry.byte_size()));

        const char *password = "toto";

        EXPECT_TRUE(writer.encrypt_all(password));
        EXPECT_TRUE(writer.close());

        // Test reading
        ZipReader reader("zip-reader");

        EXPECT_FALSE(reader.set_conf_str("password", password));

        EXPECT_TRUE(reader.open(zip_path));
        EXPECT_EQ(reader.count_entries(), 13u);

        char *data;
        size_t size;
        std::string entry_name = "toto_entry";
        EXPECT_TRUE(reader.load_entry(entry_name));
        SIHD_LOG(info, "Trying to read entry without password");
        EXPECT_TRUE(reader.read_entry() == -1);
        SIHD_LOG(info, "Trying to read entry with password");
        EXPECT_TRUE(reader.read_entry(password) > 0);
        EXPECT_TRUE(reader.get_read_data(&data, &size));
        EXPECT_STREQ(data, arr_entry.to_string().c_str());

        SIHD_LOG(info, "Setting global password");
        EXPECT_TRUE(reader.set_conf_str("password", password));
        entry_name = "toto_entry2";
        EXPECT_TRUE(reader.load_entry(entry_name));
        EXPECT_TRUE(reader.read_entry() > 0);
        EXPECT_TRUE(reader.get_read_data(&data, &size));
        EXPECT_STREQ(data, arr_entry.to_string().c_str());

        entry_name = "to_zip/file.txt";
        EXPECT_TRUE(reader.load_entry(entry_name));
        EXPECT_TRUE(reader.read_entry() > 0);
        EXPECT_TRUE(reader.get_read_data(&data, &size));
        EXPECT_STREQ(data, "hello\n");

        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestZip, test_zip_reader)
    {
        std::string zip_path = "test/resources/to_read/to_zip.zip";
        char *data;
        size_t size;
        ZipReader reader("zip-reader");

        EXPECT_TRUE(reader.open(zip_path));
        EXPECT_EQ(reader.count_entries(), 14u);

        std::string entry_name = "to_zip/file.txt";
        EXPECT_TRUE(reader.load_entry(entry_name));
        EXPECT_TRUE(reader.read_entry());
        EXPECT_TRUE(reader.get_read_data(&data, &size));
        EXPECT_STREQ(data, "hello\n");

        reader.set_read_entry_names(true);
        while (reader.read_next())
        {
            EXPECT_TRUE(reader.get_read_data(&data, &size));
            SIHD_LOG(info, "Zip entry: " << data);
            if (reader.is_entry_directory())
            {
                SIHD_LOG(info, "-> directory");
            }
            else
            {
                EXPECT_TRUE(reader.read_entry() >= 0);
                EXPECT_TRUE(reader.get_read_data(&data, &size));
                if (size > 0)
                {
                    // removing linefeed from text
                    data[size - 1] = 0;
                    SIHD_LOG(info, "-> content: " << data);
                }
                else
                    SIHD_LOG(info, "-> empty");
            }
        }

        EXPECT_TRUE(reader.close());
    }
}