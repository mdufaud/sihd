#include <sihd/http/Navigator.hpp>
#include <sihd/http/navigator/RobotsTxt.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

namespace sihd::http
{

SIHD_LOGGER;

RobotsTxt::RobotsTxt() = default;
RobotsTxt::~RobotsTxt() = default;

bool RobotsTxt::fetch(std::string_view base_url, Navigator & nav)
{
    if (_fetched && is_valid())
        return true;

    std::string url = fmt::format("{}/robots.txt", base_url);
    auto resp = nav.get(url);
    if (!resp.has_value() || resp->status() != 200)
    {
        SIHD_LOG(debug, "RobotsTxt: fetch failed for {}", url);
        return false;
    }

    parse(resp->content().cpp_str());
    _fetched = true;
    _fetched_at = std::chrono::steady_clock::now();
    return true;
}

namespace
{

std::string to_lower_str(std::string_view sv)
{
    std::string s {sv};
    return sihd::util::str::to_lower(s);
}

} // namespace

void RobotsTxt::parse(std::string_view content)
{
    _agents.clear();

    std::vector<std::string> current_agents;
    bool in_block = false;

    auto lines = sihd::util::str::split(content, "\n");

    for (const auto & line_buf : lines)
    {
        std::string_view line = sihd::util::str::trim(line_buf);

        // strip inline comment
        auto comment = line.find('#');
        if (comment != std::string_view::npos)
            line = line.substr(0, comment);
        line = sihd::util::str::trim(line);

        if (line.empty())
        {
            // empty line ends a block
            current_agents.clear();
            in_block = false;
            continue;
        }

        auto colon = line.find(':');
        if (colon == std::string_view::npos)
            continue;

        std::string key = to_lower_str(sihd::util::str::trim(line.substr(0, colon)));
        std::string_view value = sihd::util::str::trim(line.substr(colon + 1));

        if (key == "user-agent")
        {
            if (in_block)
            {
                // new agent after rules → start fresh
                current_agents.clear();
            }
            current_agents.push_back(to_lower_str(value));
            // ensure entry exists
            for (const auto & a : current_agents)
                _agents.emplace(a, AgentRules {});
        }
        else if (key == "disallow" || key == "allow")
        {
            if (!current_agents.empty())
            {
                in_block = true;
                Rule rule {key == "allow", std::string(value)};
                for (const auto & a : current_agents)
                    _agents[a].rules.push_back(rule);
            }
        }
        else if (key == "crawl-delay")
        {
            if (!current_agents.empty())
            {
                long delay = 0;
                if (sihd::util::str::to_long(value, &delay))
                {
                    for (const auto & a : current_agents)
                        _agents[a].crawl_delay_s = delay;
                }
                else
                    SIHD_LOG(warning, "RobotsTxt: invalid crawl-delay value: {}", value);
            }
        }
    }
}

void RobotsTxt::clear()
{
    _agents.clear();
    _fetched = false;
}

const RobotsTxt::AgentRules *RobotsTxt::find_rules(std::string_view user_agent) const
{
    std::string ua = to_lower_str(user_agent);

    auto it = _agents.find(ua);
    if (it != _agents.end())
        return &it->second;

    // fallback to wildcard
    auto wit = _agents.find("*");
    if (wit != _agents.end())
        return &wit->second;

    return nullptr;
}

bool RobotsTxt::is_allowed(std::string_view user_agent, std::string_view path) const
{
    const AgentRules *rules = find_rules(user_agent);
    if (rules == nullptr)
        return true; // no rules → allowed

    // RFC 9309: longest matching rule wins; allow beats disallow on equal length
    int best_len = -1;
    bool best_allow = true;

    for (const auto & rule : rules->rules)
    {
        if (rule.path.empty())
        {
            // empty Disallow means allow all
            if (!rule.allow)
            {
                if (best_len < 0)
                {
                    best_len = 0;
                    best_allow = true;
                }
            }
            continue;
        }

        if (path.starts_with(rule.path))
        {
            int len = static_cast<int>(rule.path.size());
            if (len > best_len || (len == best_len && rule.allow))
            {
                best_len = len;
                best_allow = rule.allow;
            }
        }
    }

    return best_allow;
}

std::optional<long> RobotsTxt::crawl_delay(std::string_view user_agent) const
{
    const AgentRules *rules = find_rules(user_agent);
    if (rules == nullptr)
        return std::nullopt;
    return rules->crawl_delay_s;
}

bool RobotsTxt::is_valid() const
{
    if (!_fetched)
        return false;
    auto now = std::chrono::steady_clock::now();
    return (now - _fetched_at) < _ttl;
}

void RobotsTxt::set_ttl(std::chrono::seconds ttl)
{
    _ttl = ttl;
}

} // namespace sihd::http
