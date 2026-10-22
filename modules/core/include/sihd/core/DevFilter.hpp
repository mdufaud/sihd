#ifndef __SIHD_CORE_DEVFILTER_HPP__
#define __SIHD_CORE_DEVFILTER_HPP__

#include <atomic>
#include <string_view>

#include <sihd/core/ChannelMatch.hpp>
#include <sihd/core/Device.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/Value.hpp>

namespace sihd::core
{

class DevFilter: public sihd::core::Device
{
    public:
        class Rule
        {
            public:
                Rule(ChannelMatch::Comparison comparison);
                ~Rule();

                sihd::util::Result<void> parse(std::string_view conf);
                Rule & in(std::string_view channel_name);
                Rule & out(std::string_view channel_name);
                Rule & match(bool active);
                Rule & write_same(size_t idx);
                // must be called after setting trigger index with 'trigger' method
                Rule & write_same();
                Rule & delay(sihd::util::Duration nano_delay);
                // delay write by seconds.milliseconds
                Rule & delay(double delay);

                Rule & trigger(size_t idx, sihd::util::Value val);
                Rule & write(size_t idx, sihd::util::Value val);

                ChannelMatch channel_match;
                std::string channel_in;
                std::string channel_out;
                bool write_same_value;
                size_t write_idx;
                sihd::util::Value write_value;
                sihd::util::Duration nano_delay;
        };

        DevFilter(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevFilter();

        bool set_filter_equal(std::string_view rule_str);
        bool set_filter_superior(std::string_view rule_str);
        bool set_filter_superior_equal(std::string_view rule_str);
        bool set_filter_inferior(std::string_view rule_str);
        bool set_filter_inferior_equal(std::string_view rule_str);
        bool set_filter_byte_and(std::string_view rule_str);
        bool set_filter_byte_or(std::string_view rule_str);
        bool set_filter_byte_xor(std::string_view rule_str);

        void set_filter(const Rule & rule);

        bool is_running() const override;

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *c) override;

        bool on_setup() override;
        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

        void _rule_match(Channel *channel_out, const Rule *rule_ptr, int64_t out_val);

    private:
        class DelayWriter: public sihd::util::Task
        {
            public:
                DelayWriter(DevFilter *dev, Channel *channel_out, const Rule *rule_ptr, int64_t out_val);
                ~DelayWriter();

                bool run();

                DevFilter *dev;
                Channel *channel_out;
                const Rule *rule_ptr;
                int64_t out_val;
        };

        struct InternalRule
        {
                InternalRule();
                ~InternalRule();

                sihd::util::Result<void> set(const Rule *conf, Channel *in, Channel *out);
                sihd::util::Result<void> verify();

                Channel *channel_in_ptr;
                Channel *channel_out_ptr;
                const Rule *rule_ptr;
        };

        bool _set_filter_conf(std::string_view conf, ChannelMatch::Comparison comparison);
        sihd::util::Result<void> _parse_conf(std::string_view conf, ChannelMatch::Comparison comparison);
        void _apply_rule(const Channel *channel_in, Channel *channel_out, const Rule *rule_ptr);

        std::atomic<bool> _running;
        std::mutex _run_mutex;
        std::vector<Rule> _rules_lst;
        std::map<Channel *, std::vector<std::unique_ptr<InternalRule>>> _rules_map;
        sihd::util::Scheduler *_scheduler_ptr;
        bool _rule_with_delay;
};

} // namespace sihd::core

#endif
