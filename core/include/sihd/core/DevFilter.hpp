#ifndef __SIHD_CORE_DEVFILTER_HPP__
#define __SIHD_CORE_DEVFILTER_HPP__

#include <sihd/util/Scheduler.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/Value.hpp>

#include <sihd/core/Device.hpp>

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

        class Rule
        {
            public:
                Rule(RuleType type);
                ~Rule();

                bool parse(std::string_view conf);
                Rule & in(std::string_view channel_name);
                Rule & out(std::string_view channel_name);
                // if rule should match or not match
                Rule & match(bool active);
                // write trigger value at channel's output idx
                Rule & write_same(size_t idx);
                // must be called after setting trigger index with 'trigger' method
                Rule & write_same();
                // delay write by X nanoseconds
                Rule & delay(time_t nano_delay);
                // delay write by seconds.milliseconds
                Rule & delay(double delay);

                template <typename T>
                Rule & trigger(size_t idx, T val)
                {
                    this->trigger_idx = idx;
                    this->trigger_value.set<T>(val);
                    return *this;
                }

                template <typename T>
                Rule & write(size_t idx, T val)
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
                bool should_match;
                time_t nano_delay;
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

                bool set(const Rule *conf, Channel *in, Channel *out);
                bool verify();

                Channel *channel_in_ptr;
                Channel *channel_out_ptr;
                const Rule *rule_ptr;
        };

        bool _parse_conf(std::string_view conf, RuleType type);
        void _apply_rule(const Channel *channel_in, Channel *channel_out, const Rule *rule_ptr);

        bool _running;
        std::mutex _run_mutex;
        std::vector<Rule> _rules_lst;
        std::map<Channel *, std::vector<std::unique_ptr<InternalRule>>> _rules_map;
        sihd::util::Scheduler *_scheduler_ptr;
        bool _rule_with_delay;
};

} // namespace sihd::core

#endif