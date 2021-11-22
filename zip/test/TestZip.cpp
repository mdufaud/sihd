#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/zip/ZipWriter.hpp>
#include <sihd/zip/ZipReader.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/Array.hpp>

namespace test
{
    NEW_LOGGER("test");
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

        LOG(info, "Creating zip archive: " << zip_path);
        EXPECT_TRUE(writer.open(zip_path));

        // Adding fs directory
        std::string entry = "test/resources/to_zip";
        LOG(info, "Zipping: " << entry);
        EXPECT_TRUE(writer.fs_add(entry, "to_zip"));

        // Adding array entry
        sihd::util::ArrStr arr_entry("hello test world");
        EXPECT_TRUE(writer.add_file("toto_entry", arr_entry.cbuf(), arr_entry.byte_size()));

        EXPECT_TRUE(writer.encrypt_all("toto"));
        EXPECT_TRUE(writer.close());

        // Test reading
        ZipReader reader("zip-reader");

        EXPECT_FALSE(reader.set_conf("password", "toto"));

        EXPECT_TRUE(reader.open(zip_path));
        EXPECT_EQ(reader.count_entries(), 15u);

        EXPECT_TRUE(reader.set_conf("password", "toto"));
        
        // Testing fs directory entry
        std::string entry_name = "to_zip/file.txt";
        zip_stat_t zip_entry;
        EXPECT_TRUE(reader.get_entry(entry_name, &zip_entry));
        EXPECT_EQ(zip_entry.name, entry_name);

        std::string entry_text;
        EXPECT_TRUE(reader.read_entry_into(&zip_entry, entry_text));

        EXPECT_EQ(entry_text, "hello\n");

        // Testing array entry
        entry_name = "toto_entry";
        EXPECT_TRUE(reader.get_entry(entry_name, &zip_entry));
        EXPECT_EQ(zip_entry.name, entry_name);
        entry_text.clear();
        EXPECT_TRUE(reader.read_entry_into(&zip_entry, entry_text));
        EXPECT_EQ(entry_text, arr_entry.to_string());

        EXPECT_TRUE(reader.close());
    }

    TEST_F(TestZip, test_zip_reader)
    {
        std::string zip_path = "test/resources/to_read/to_zip.zip";
        ZipReader reader("zip-reader");

        EXPECT_TRUE(reader.open(zip_path));
        EXPECT_EQ(reader.count_entries(), 14u);

        std::string entry_name = "to_zip/file.txt";
        zip_stat_t zip_entry;
        EXPECT_TRUE(reader.get_entry(entry_name, &zip_entry));
        EXPECT_EQ(zip_entry.name, entry_name);

        std::vector<zip_stat_t> entries;
        EXPECT_TRUE(reader.get_entries(entries));
        for (const auto & entry: entries)
        {
            LOG(debug, entry.name);
        }

        std::string entry_text;
        EXPECT_TRUE(reader.read_entry_into(&zip_entry, entry_text));

        EXPECT_EQ(entry_text, "hello\n");

        EXPECT_TRUE(reader.close());
    }
}