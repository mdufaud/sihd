#include <type_traits>

#include <gtest/gtest.h>

#include <sihd/util/TokenPattern.hpp>

namespace test
{
using namespace sihd::util;

class TestTokenPattern: public ::testing::Test
{
    protected:
        TestTokenPattern() = default;
        virtual ~TestTokenPattern() = default;
};

TEST_F(TestTokenPattern, test_tokenpattern_compile_tokens)
{
    TokenPattern pattern;
    ASSERT_TRUE(pattern.compile("pre {first} {{ {second:spec} [{third:>3}]}}").has_value());

    const std::span<const TokenPattern::Token> tokens = pattern.tokens();
    ASSERT_EQ(tokens.size(), 10u);

    EXPECT_TRUE(tokens[0].name.empty());
    EXPECT_EQ(tokens[0].text, "pre ");

    EXPECT_EQ(tokens[1].name, "first");
    EXPECT_TRUE(tokens[1].raw_spec.empty());
    EXPECT_EQ(tokens[1].offset, 4u);
    EXPECT_TRUE(tokens[1].classical);

    EXPECT_EQ(tokens[3].text, "{");

    EXPECT_EQ(tokens[5].name, "second");
    EXPECT_EQ(tokens[5].raw_spec, "spec");
    // a strftime-like spec is not classical: raw for the consumer
    EXPECT_FALSE(tokens[5].classical);

    EXPECT_EQ(tokens[6].text, " [");

    EXPECT_EQ(tokens[7].name, "third");
    EXPECT_EQ(tokens[7].raw_spec, ">3");
    EXPECT_TRUE(tokens[7].classical);
    EXPECT_EQ(tokens[7].options.width, 3u);
    EXPECT_EQ(tokens[7].options.align, '>');

    EXPECT_EQ(tokens[9].text, "}");

    EXPECT_EQ(pattern.pattern(), "pre {first} {{ {second:spec} [{third:>3}]}}");
}

TEST_F(TestTokenPattern, test_tokenpattern_classical_options)
{
    TokenPattern pattern;
    ASSERT_TRUE(pattern.compile("{a:*>7}|{b:^7}|{c:.3}|{d:9}|{e:<<7}").has_value());

    const std::span<const TokenPattern::Token> tokens = pattern.tokens();
    ASSERT_EQ(tokens.size(), 9u);

    EXPECT_EQ(tokens[0].options.fill, '*');
    EXPECT_EQ(tokens[0].options.align, '>');
    EXPECT_EQ(tokens[0].options.width, 7u);

    EXPECT_EQ(tokens[2].options.align, '^');
    EXPECT_EQ(tokens[2].options.width, 7u);

    EXPECT_EQ(tokens[4].options.precision, 3u);
    EXPECT_EQ(tokens[4].options.width, 0u);

    // no align: std::format strings default to left
    EXPECT_EQ(tokens[6].options.width, 9u);
    EXPECT_EQ(tokens[6].options.align, '<');

    // the fill may be an align char: the second one is the align
    EXPECT_EQ(tokens[8].options.fill, '<');
    EXPECT_EQ(tokens[8].options.align, '<');
    EXPECT_EQ(tokens[8].options.width, 7u);
}

TEST_F(TestTokenPattern, test_tokenpattern_render)
{
    TokenPattern pattern;
    ASSERT_TRUE(pattern.compile("hello {name:*>10}|{name:^7}|{name:.2}|{age}").has_value());

    std::string out;
    pattern.render(out, [](const TokenPattern::Token & token) -> std::string_view {
        if (token.name == "name")
            return "bob";
        if (token.name == "age")
            return "1337";
        return {};
    });
    EXPECT_EQ(out, "hello *******bob|  bob  |bo|1337");
}

TEST_F(TestTokenPattern, test_tokenpattern_move_keeps_views)
{
    static_assert(std::is_move_constructible_v<TokenPattern>);
    static_assert(std::is_move_assignable_v<TokenPattern>);
    static_assert(std::is_copy_constructible_v<TokenPattern> == false);

    TokenPattern pattern;
    ASSERT_TRUE(pattern.compile("{one} {two}").has_value());

    TokenPattern moved = std::move(pattern);
    EXPECT_EQ(moved.tokens()[0].name, "one");
    EXPECT_EQ(moved.tokens()[2].name, "two");

    ASSERT_FALSE(moved.compile("{nope").has_value());
    EXPECT_EQ(moved.pattern(), "{one} {two}");
    EXPECT_EQ(moved.tokens()[0].name, "one");
}

TEST_F(TestTokenPattern, test_tokenpattern_refused_patterns)
{
    const std::vector<std::string> bad_patterns = {
        "trailing {",
        "stray }",
        "{}",
        "{a:99999999}",
        "{a:unterminated",
    };
    for (const std::string & bad : bad_patterns)
    {
        TokenPattern pattern;
        const std::expected<void, Error> res = pattern.compile(bad);
        ASSERT_FALSE(res.has_value()) << bad;
        EXPECT_NE(res.error().message.find("at "), std::string::npos) << bad;
    }

    TokenPattern pattern;
    EXPECT_TRUE(pattern.compile("{a:4096}").has_value());
    EXPECT_FALSE(pattern.compile("{a:4097}").has_value());
}

} // namespace test
