#ifndef __SIHD_HTTP_ROBOTSTXT_HPP__
#define __SIHD_HTTP_ROBOTSTXT_HPP__

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sihd::http
{

class Navigator;

class RobotsTxt
{
    public:
        RobotsTxt();
        ~RobotsTxt();

        // fetch and parse robots.txt from base_url (e.g. "https://example.com")
        // returns false if fetch failed or returned non-200
        bool fetch(std::string_view base_url, Navigator & nav);

        // parse raw robots.txt content
        void parse(std::string_view content);

        // reset all parsed data
        void clear();

        // check if user_agent is allowed to access path
        bool is_allowed(std::string_view user_agent, std::string_view path) const;

        // returns the Crawl-delay for the given user agent (in seconds)
        std::optional<long> crawl_delay(std::string_view user_agent) const;

        // returns true if the cached content is still valid (within TTL)
        bool is_valid() const;

        void set_ttl(std::chrono::seconds ttl);

    private:
        struct Rule
        {
            bool allow;
            std::string path;
        };

        struct AgentRules
        {
            std::vector<Rule> rules;
            std::optional<long> crawl_delay_s;
        };

        std::unordered_map<std::string, AgentRules> _agents;
        std::chrono::seconds _ttl{3600};
        std::chrono::steady_clock::time_point _fetched_at{};
        bool _fetched = false;

        const AgentRules *find_rules(std::string_view user_agent) const;
};

} // namespace sihd::http

#endif
