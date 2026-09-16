#include <gtest/gtest.h>

#include <sihd/http/MimeTypes.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::http;
class TestMimeTypes: public ::testing::Test
{
    protected:
        TestMimeTypes() { sihd::util::LoggerManager::stream(); }

        virtual ~TestMimeTypes() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestMimeTypes, test_mime_known_extensions)
{
    MimeTypes mime;

    EXPECT_EQ(mime.get("png"), MimeTypes::MIME_IMAGE_PNG);
    EXPECT_EQ(mime.get("html"), MimeTypes::MIME_TEXT_HTML);
    EXPECT_EQ(mime.get("json"), MimeTypes::MIME_APPLICATION_JSON);
}

TEST_F(TestMimeTypes, test_mime_case_insensitive)
{
    MimeTypes mime;

    EXPECT_EQ(mime.get("PNG"), MimeTypes::MIME_IMAGE_PNG);
    EXPECT_EQ(mime.get("Html"), MimeTypes::MIME_TEXT_HTML);

    mime.add("WASM", "application/wasm");
    EXPECT_EQ(mime.get("wasm"), "application/wasm");
}

TEST_F(TestMimeTypes, test_mime_unknown_or_empty_extension)
{
    MimeTypes mime;

    // an unknown or missing extension must not pretend to be text
    EXPECT_EQ(mime.get("exe"), MimeTypes::MIME_APPLICATION_OCTET);
    EXPECT_EQ(mime.get("unknownext"), MimeTypes::MIME_APPLICATION_OCTET);
    EXPECT_EQ(mime.get(""), MimeTypes::MIME_APPLICATION_OCTET);
}

} // namespace test
