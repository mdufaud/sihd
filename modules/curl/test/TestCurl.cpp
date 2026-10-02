#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/curl.hpp>
#include <sihd/util/str.hpp>

using enum sihd::util::ErrorCode;

namespace test
{

using namespace sihd::curl;

namespace
{

// run-scoped directory: parallel test runs never share files
class TempDir
{
    public:
        TempDir()
        {
            const std::string suffix = std::to_string(std::random_device {}());
            std::filesystem::path candidate = std::filesystem::temp_directory_path() / ("sihd_curl_test_" + suffix);
            if (std::filesystem::create_directory(candidate))
                _path = candidate;
        }

        ~TempDir() { std::filesystem::remove_all(_path); }

        const std::filesystem::path & path() const { return _path; }

    private:
        std::filesystem::path _path;
};

std::string make_file_url(const TempDir & dir, const std::string & filename, const std::string & content)
{
    std::filesystem::path path = dir.path() / filename;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << content;
    return "file:///" + std::filesystem::absolute(path).generic_string();
}

std::string read_file(const TempDir & dir, const std::string & filename)
{
    std::ifstream file(dir.path() / filename, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

} // namespace

TEST(TestCurl, test_version)
{
    EXPECT_TRUE(version().rfind("libcurl/", 0) == 0);
}

TEST(TestCurl, test_request_file_transfer)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());
    const std::string content = "hello sihd curl";
    const std::string url = make_file_url(dir, "transfer.txt", content);

    Request request;
    std::string received;
    request.set_write_callback([&received](sihd::util::ArrByteView data) {
        received.append((const char *)data.buf(), data.size());
        return true;
    });
    request.set_url(url);
    EXPECT_TRUE(request.perform().has_value());
    EXPECT_EQ(received, content);
    EXPECT_TRUE(request.content_type().empty());
}

TEST(TestCurl, test_request_upload)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());
    const std::string content = "uploaded by sihd";
    const std::string url = "file:///" + std::filesystem::absolute(dir.path() / "upload.txt").generic_string();

    Request request;
    size_t offset = 0;
    request.set_read_callback([&content, &offset](char *buffer, size_t capacity) {
        size_t count = std::min(capacity, content.size() - offset);
        std::memcpy(buffer, content.data() + offset, count);
        offset += count;
        return count;
    });
    EXPECT_TRUE(request.set_upload(true));
    EXPECT_TRUE(request.set_infilesize((int64_t)content.size()));
    request.set_url(url);
    EXPECT_TRUE(request.perform().has_value());
    EXPECT_EQ(offset, content.size());
    EXPECT_EQ(read_file(dir, "upload.txt"), content);
}

TEST(TestCurl, test_request_move)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());
    const std::string content = "moved request";
    const std::string url = make_file_url(dir, "move.txt", content);

    Request request;
    request.set_url(url);

    Request moved(std::move(request));
    std::string received;
    moved.set_write_callback([&received](sihd::util::ArrByteView data) {
        received.append((const char *)data.buf(), data.size());
        return true;
    });
    EXPECT_TRUE(moved.perform().has_value());
    EXPECT_EQ(received, content);

    // the moved-from request fails every operation without crashing
    EXPECT_FALSE(request.set_url(url));
    auto defunct = request.perform();
    ASSERT_FALSE(defunct.has_value());
    EXPECT_EQ(defunct.error().code, not_initialized);
    request.reset();

    Request target;
    target = std::move(moved);
    EXPECT_TRUE(target.perform().has_value());
}

TEST(TestCurl, test_request_write_abort)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());
    const std::string content = "0123456789";
    const std::string url = make_file_url(dir, "abort.txt", content);

    Request request;
    size_t received = 0;
    request.set_write_callback([&received](sihd::util::ArrByteView data) {
        received += data.size();
        return false;
    });
    request.set_url(url);
    EXPECT_FALSE(request.perform().has_value());
    EXPECT_GT(received, 0u);
}

TEST(TestCurl, test_request_error)
{
    Request request;
    // environment proxies would answer for the unreachable endpoint
    EXPECT_TRUE(request.set_proxy(""));
    request.set_url("http://127.0.0.1:1/");
    request.set_connect_timeout(sihd::util::Duration(std::chrono::milliseconds(500)));
    auto failed = request.perform();
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error().code, io_error);
    EXPECT_FALSE(request.last_error().empty());
}

TEST(TestCurl, test_request_cookies)
{
    Request request;
    EXPECT_TRUE(request.set_cookie_engine());
    EXPECT_TRUE(request.add_cookie("localhost\tFALSE\t/\tFALSE\t0\tsihd\tvalue"));

    std::vector<std::string> lines = request.cookie_list();
    ASSERT_EQ(lines.size(), 1u);
    auto parts = sihd::util::str::split(lines[0], "\t");
    ASSERT_GE(parts.size(), 7u);
    EXPECT_EQ(parts[5], "sihd");
    EXPECT_EQ(parts[6], "value");

    EXPECT_TRUE(request.clear_cookies());
    EXPECT_TRUE(request.cookie_list().empty());
}

TEST(TestCurl, test_request_options)
{
    Request request;
    EXPECT_TRUE(request.set_verbose(false));
    EXPECT_TRUE(request.set_follow_location(true));
    EXPECT_TRUE(request.set_timeout(sihd::util::Duration(std::chrono::seconds(3))));
    EXPECT_TRUE(request.set_user_agent("sihd"));
    EXPECT_TRUE(request.set_ssl_verify(true, true));
    EXPECT_TRUE(request.set_userpass("user", "pass"));
    EXPECT_TRUE(request.set_auth(Auth::Basic));
    EXPECT_TRUE(request.set_proxy("http://localhost:8080", Proxy::Http));
    EXPECT_TRUE(request.set_proxy_auth("user", "pass"));
    EXPECT_TRUE(request.set_body("body"));
    EXPECT_TRUE(request.set_custom_request("GET"));
    EXPECT_TRUE(request.set_accept_encoding(""));
    EXPECT_TRUE(request.set_http_2_tls(false));
    request.reset();
}

TEST(TestCurl, test_header_list)
{
    HeaderList list;
    EXPECT_TRUE(list.empty());
    EXPECT_TRUE(list.append("Content-Type: application/json"));
    EXPECT_TRUE(list.append("Accept: text/html"));
    EXPECT_EQ(list.size(), 2u);

    std::vector<std::string> lines = list.lines();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "Content-Type: application/json");
    EXPECT_EQ(lines[1], "Accept: text/html");

    HeaderList moved(std::move(list));
    EXPECT_EQ(moved.size(), 2u);
    EXPECT_TRUE(list.empty());

    HeaderList assigned;
    assigned = std::move(moved);
    EXPECT_EQ(assigned.size(), 2u);
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(moved.size(), 0u);
}

TEST(TestCurl, test_mime)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());

    Request request;
    Mime mime;

    std::vector<uint8_t> binary = {0, 1, 2, 255};
    mime.add_part().name("field").data("value");
    mime.add_part().name("bin").data(binary);
    const std::filesystem::path part_file = dir.path() / "part.txt";
    {
        std::ofstream out(part_file);
        out << "mime file";
    }
    mime.add_part().name("file").file(part_file.string()).filename("renamed.txt").content_type("text/plain");

    EXPECT_TRUE(request.set_mime(mime));
    // a file:// transfer never produces the mime body: the wire format is
    // asserted by the http module tests against a real server
    request.set_url(make_file_url(dir, "mime.txt", "unused"));
    request.set_write_callback([](sihd::util::ArrByteView) { return true; });
    EXPECT_TRUE(request.perform().has_value());

    // a default-constructed part has no target: its setters do not crash
    Mime::Part empty;
    empty.name("x").data("y").content_type("text/plain");
}

} // namespace test
