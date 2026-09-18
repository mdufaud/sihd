#ifndef __SIHD_CURL_MIME_HPP__
#define __SIHD_CURL_MIME_HPP__

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sihd::curl
{

// one multipart part as plain data, materialized by Request::set_mime
struct MimePart
{
        std::string name;
        std::vector<uint8_t> data;
        std::string path;
        std::string filename;
        std::string content_type;
};

class Mime
{
    public:
        class Part
        {
            public:
                Part() = default;
                explicit Part(MimePart *part);

                Part & name(std::string_view name);
                Part & data(std::string_view data);
                Part & data(const std::vector<uint8_t> & data);
                Part & file(std::string_view path);
                Part & filename(std::string_view filename);
                Part & content_type(std::string_view content_type);

            private:
                MimePart *_part = nullptr;
        };

        Mime() = default;
        ~Mime() = default;

        Mime(Mime &&) = default;
        Mime & operator=(Mime &&) = default;

        Part add_part();

        const std::vector<MimePart> & parts() const { return _parts; }

    private:
        std::vector<MimePart> _parts;
};

} // namespace sihd::curl

#endif
