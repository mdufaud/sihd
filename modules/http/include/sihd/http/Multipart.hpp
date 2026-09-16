#ifndef __SIHD_HTTP_MULTIPART_HPP__
#define __SIHD_HTTP_MULTIPART_HPP__

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sihd::http
{

class Multipart
{
    public:
        struct Part
        {
                std::string name;
                std::string filename;
                std::string content_type;
                // field value, or content of a parsed file
                std::string data;
                // file streamed from disk when sending, data stays empty
                std::string path;

                bool is_file() const;
        };

        // case-insensitive match of the multipart/form-data media type, parameters allowed
        static bool is_content_type(std::string_view content_type);
        static std::optional<std::string> boundary(std::string_view content_type);
        static std::optional<Multipart> parse(std::string_view body, std::string_view content_type);

        void add_field(std::string name, std::string value, std::string content_type = {});
        void add_file(std::string name, std::string path, std::string filename = {}, std::string content_type = {});

        void clear();
        bool empty() const;
        std::optional<std::string_view> value(std::string_view name) const;
        const Part *file(std::string_view name) const;
        const std::vector<Part> & parts() const;

    private:
        std::vector<Part> _parts;
};

} // namespace sihd::http

#endif
