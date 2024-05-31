#ifndef __SIHD_UTIL_DYNMESSAGE_HPP__
#define __SIHD_UTIL_DYNMESSAGE_HPP__

#include <functional>

#include <sihd/util/Message.hpp>
#include <sihd/util/forward.hpp>

namespace sihd::util
{

class DynMessage: public Message
{
    public:
        using RuleCallback = std::function<void(DynMessage &, const IMessageField &)>;

        DynMessage(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DynMessage() = default;

        virtual size_t field_byte_size() const override { return _total_dyn_size; };
        virtual bool finish() override;

        virtual bool field_assign_buffer(void *buffer) override;
        virtual bool field_read_from(const void *buffer, size_t size) override;
        virtual bool field_write_to(void *buffer, size_t size) override;
        virtual bool field_resize(size_t size) override;

        bool add_rule(const std::string & field_name, RuleCallback && callback);
        bool remove_rules(const std::string & field_name);

        bool hide_field(const std::string & name, bool active);

        virtual IMessageField *clone() const override;

    protected:
        std::vector<IMessageField *> _hidden_fields;
        std::unordered_map<std::string, std::vector<RuleCallback>> _rules;

    private:
        bool _is_hidden(const IMessageField *field) const;
        void _play_rules(const std::string & name, const IMessageField *field);

        size_t _total_dyn_size;
};

} // namespace sihd::util

#endif