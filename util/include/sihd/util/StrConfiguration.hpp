#ifndef __SIHD_UTIL_STRCONFIGURATION_HPP__
#define __SIHD_UTIL_STRCONFIGURATION_HPP__

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sihd::util
{

class StrConfiguration
{
    public:
        StrConfiguration(std::string_view conf);
        ~StrConfiguration() = default;

        void parse_configuration(std::string_view conf);

        size_t size() const;
        bool has(const std::string & key) const;
        std::optional<std::string> find(const std::string & key) const;

        // returns empty if not found
        std::string operator[](const std::string & key) const;

        // throws out of range if not found
        std::string get(const std::string & key) const;

        std::string str() const;

        template <typename... T>
        std::array<std::optional<std::string>, sizeof...(T)> find_all(const T &...args) const
        {
            std::array<std::optional<std::string>, sizeof...(T)> array;

            int i = 0;
            auto finder = [&array, &i, this](const auto & arg) {
                array[i] = this->find(arg);
                ++i;
            };

            (finder(args), ...);

            return array;
        }

    protected:

    private:
        std::unordered_map<std::string, std::string> _configuration;
};

} // namespace sihd::util

#endif
