#ifndef __SIHD_CORE_DEVFILTER_HPP__
# define __SIHD_CORE_DEVFILTER_HPP__

# include <sihd/core/Device.hpp>

namespace sihd::core
{

class DevFilter:   public sihd::core::Device
{
    public:
        DevFilter(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevFilter();

        bool set_filter_equal(const std::string & conf);
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
            none,
            superior,
            equal,
        };

        struct RuleConf
        {
            RuleType type;
            std::string channel_in_path;
            std::string channel_out_path;
            std::string trigger;
            std::string write;
        };

        struct Rule
        {
            Rule();
            ~Rule();

            bool parse(const RuleConf & conf, Channel *in, Channel *out);

            RuleType type;
            Channel *channel_in_ptr;
            Channel *channel_out_ptr;
            bool write_same_value;
            size_t trigger_idx;
            int64_t trigger_value;
            size_t write_idx;
            int64_t write_value;
        };

        bool _parse_conf(const std::string & conf, RuleConf & rule_to_fill);
        void _handle_int(Channel *channel_in, Rule *rule_ptr);
        void _handle_float(Channel *channel_in, Rule *rule_ptr);
        void _matched(Channel *channel_out, Rule *rule, int64_t out_val);

        bool _running;
        std::mutex _run_mutex;
        std::vector<RuleConf> _rules_lst;
        std::map<Channel *, std::unique_ptr<Rule>> _rules_map;
};

}

#endif