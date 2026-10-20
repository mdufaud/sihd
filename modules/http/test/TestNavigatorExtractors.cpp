#include <gtest/gtest.h>

#include <sihd/http/navigator/CsrfExtractor.hpp>
#include <sihd/http/navigator/LinkExtractor.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

#include "http_test_helpers.hpp"

namespace test
{

using namespace sihd::http;
using namespace sihd::util;

class TestNavigatorExtractors: public ::testing::Test
{
    protected:
        TestNavigatorExtractors() { LoggerManager::stream(); }
        virtual ~TestNavigatorExtractors() { LoggerManager::clear_loggers(); }
};

TEST_F(TestNavigatorExtractors, test_link_extractor_hostile_html)
{
    std::string html = "<a ";
    for (int i = 0; i < 2000; ++i)
        html += "href=\"a\" ";

    Stopwatch stopwatch;
    const auto links = extract_links(html, "https://mysite.com");
    const Duration elapsed = stopwatch.time();

    EXPECT_TRUE(links.empty());
    if constexpr (build::is_run_with_sanitizer == false)
    {
        EXPECT_LT(elapsed, Duration(time::sec(1)));
    }
}

TEST_F(TestNavigatorExtractors, test_link_extractor_markup_variants)
{
    const std::string
        html = R"(<A HREF='/upper'>Upper</A><a class=x href=/bare>Bare</a><abbr href="/not-a-link">no</abbr>)";

    const auto links = extract_links(html, "https://mysite.com");

    ASSERT_EQ(links.size(), 2u);
    EXPECT_EQ(links[0].url, "https://mysite.com/upper");
    EXPECT_EQ(links[0].text, "Upper");
    EXPECT_EQ(links[1].url, "https://mysite.com/bare");
    EXPECT_EQ(links[1].text, "Bare");
}

TEST_F(TestNavigatorExtractors, test_csrf_extractor_hostile_html)
{
    std::string html = "<input ";
    for (int i = 0; i < 1500; ++i)
        html += "name=\"_csrf\" ";

    Stopwatch stopwatch;
    const CsrfResult result = csrf_extract_from_html(html);
    const Duration elapsed = stopwatch.time();

    EXPECT_TRUE(result.value.empty());
    if constexpr (build::is_run_with_sanitizer == false)
    {
        EXPECT_LT(elapsed, Duration(time::sec(1)));
    }
}

TEST_F(TestNavigatorExtractors, test_csrf_extractor_exact_field_name)
{
    const std::string
        html = R"(<input type="hidden" name="_csrf_other" value="wrong"><input name='_csrf' type=hidden value=right>)";

    const CsrfResult result = csrf_extract_from_html(html);

    EXPECT_EQ(result.field_name, "_csrf");
    EXPECT_EQ(result.value, "right");
}

} // namespace test
