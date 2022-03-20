#ifndef __SIHD_CORE_DEVFILTER_HPP__
# define __SIHD_CORE_DEVFILTER_HPP__

# include <sihd/core/Device.hpp>
# include <sihd/util/Value.hpp>

namespace sihd::core
{

class DevFilter:   public sihd::core::Device
{
    public:
        DevFilter(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevFilter();

        bool set_filter_equal(const std::string & conf);
        bool set_filter_superior(const std::string & conf);
        bool set_filter_superior_equal(const std::string & conf);
        bool set_filter_inferior(const std::string & conf);
        bool set_filter_inferior_equal(const std::string & conf);
        bool set_filter_byte_and(const std::string & conf);
        bool set_filter_byte_or(const std::string & conf);
        bool set_filter_byte_xor(const std::string & conf);

        bool is_running() const override;

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *c) override;

        bool on_setup() override;
        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

    private:
        enum RuleType
        {
            NONE,
            EQUAL,
            SUPERIOR,
            SUPERIOR_EQUAL,
            INFERIOR,
            INFERIOR_EQUAL,
            BYTE_AND,
            BYTE_OR,
            BYTE_XOR,
        };

        struct RuleConf
        {
            RuleType type;
            std::map<std::string, std::string> conf_map;
        };

        struct Rule
        {
            Rule();
            ~Rule();

            bool parse(const RuleConf & conf, Channel *in, Channel *out);
            bool parse_trigger_config(const RuleConf & conf);
            bool parse_write_config(const RuleConf & conf);
            bool parse_options_config(const RuleConf & conf);
            bool verify_parsed();

            RuleType type;
            Channel *channel_in_ptr;
            Channel *channel_out_ptr;
            bool write_same_value;
            size_t trigger_idx;
            sihd::util::Value trigger_value;
            size_t write_idx;
            sihd::util::Value write_value;
            bool notify_if_same;
            bool should_match;
        };

        bool _parse_conf(const std::string & conf, RuleConf & rule_to_fill);
        void _matched(Channel *channel_out, Rule *rule, int64_t out_val);
        void _apply_rule(Channel *channel, Rule *rule);

        bool _running;
        std::mutex _run_mutex;
        std::vector<RuleConf> _rules_lst;
        std::map<Channel *, std::vector<std::unique_ptr<Rule>>> _rules_map;
};

}

#endif