#ifndef __SIHD_CURL_MIME_HPP__
#define __SIHD_CURL_MIME_HPP__

#include <cstdint>
#include <string_view>
#include <vector>

namespace sihd::curl
{

struct MimeImpl;
struct MimePartImpl;

class Request;

class Mime
{
    public:
        class Part
        {
            public:
                Part & name(std::string_view name);
                Part & data(std::string_view data);
                Part & data(const std::vector<uint8_t> & data);
                Part & file(std::string_view path);
                Part & filename(std::string_view filename);
                Part & content_type(std::string_view content_type);

            private:
                friend class Mime;
                Part() = default;
                Part(MimePartImpl *part): _part(part) {}

                MimePartImpl *_part = nullptr;
        };

        Mime();
        ~Mime();

        Mime(Mime &&);
        Mime & operator=(Mime &&);

        Part add_part();

    private:
        friend class Request;
        MimeImpl *_impl = nullptr;
};

} // namespace sihd::curl

#endif
