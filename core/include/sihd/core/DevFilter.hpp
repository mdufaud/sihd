#ifndef __SIHD_CORE_DEVFILTER_HPP__
# define __SIHD_CORE_DEVFILTER_HPP__

# include <sihd/core/Device.hpp>
# include <sihd/util/Value.hpp>

namespace sihd::core
{

class DevFilter: public sihd::core::Device
{
    public:
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

        class RuleConf
        {
            public:
                RuleConf(RuleType type);
                ~RuleConf();

                bool parse(const std::string & conf);
                RuleConf & in(std::string_view channel_name);
                RuleConf & out(std::string_view channel_name);
                // notify if same value is wrote in channel's output
                RuleConf & notify_same(bool active);
                // if rule should match or not match
                RuleConf & match(bool active);
                // write trigger value at channel's output idx
                RuleConf & write_same(size_t idx);
                // must be called after setting trigger index with 'trigger' method
                RuleConf & write_same();

                template <typename T>
                RuleConf & trigger(size_t idx, T val)
                {
                    this->trigger_idx = idx;
                    this->trigger_value.set<T>(val);
                    return *this;
                }

                template <typename T>
                RuleConf & write(size_t idx, T val)
                {
                    this->write_idx = idx;
                    this->write_same_value = false;
                    this->write_value.set<T>(val);
                    return *this;
                }

                RuleType type;
                // channels name
                std::string channel_in;
                std::string channel_out;
                // trigger
                size_t trigger_idx;
                sihd::util::Value trigger_value;
                // write
                bool write_same_value;
                size_t write_idx;
                sihd::util::Value write_value;
                // options
                bool notify_if_same;
                bool should_match;

            private:
                bool _parse_trigger_config(const std::map<std::string, std::string> & conf);
                bool _parse_write_config(const std::map<std::string, std::string> & conf);
                bool _parse_options_config(const std::map<std::string, std::string> & conf);
        };

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

        void set_rule(const RuleConf & conf);

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
        struct Rule
        {
            Rule();
            ~Rule();

            bool set(const RuleConf *conf, Channel *in, Channel *out);
            bool verify();

            Channel *channel_in_ptr;
            Channel *channel_out_ptr;
            const RuleConf *conf_ptr;
        };

        bool _parse_conf(const std::string & conf, RuleType type);
        void _matched(Channel *channel_out, const RuleConf *conf_ptr, int64_t out_val);
        void _apply_rule(const Channel *channel_in, Channel *channel_out, const RuleConf *conf_ptr);

        bool _running;
        std::mutex _run_mutex;
        std::vector<RuleConf> _rules_lst;
        std::map<Channel *, std::vector<std::unique_ptr<Rule>>> _rules_map;
};

}

#endif