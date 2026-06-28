#include <algorithm>

#include <sihd/util/DynMessage.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_LOGGER;

DynMessage::DynMessage(const std::string & name, sihd::util::Node *parent): sihd::util::Message(name, parent)
{
    _next_rule_id = 0;
    _total_dyn_size = 0;
}

bool DynMessage::finish()
{
    if (this->is_finished())
        return false;
    for (const auto & name : this->children_keys())
    {
        IMessageField *field = _fields.at(name);
        field->field_resize(field->field_size());
    }
    _finished = true;
    return true;
}

bool DynMessage::field_resize(size_t size)
{
    (void)size;
    return true;
}

bool DynMessage::field_assign_buffer(void *buffer)
{
    (void)buffer;
    return false;
}

bool DynMessage::remove_rules(const std::string & field_name)
{
    return _rules.erase(field_name) > 0;
}

bool DynMessage::remove_rule(const std::string & field_name, size_t rule_id)
{
    const auto it = _rules.find(field_name);
    if (it == _rules.end())
        return false;
    auto & vec = it->second;
    const auto eit = std::find_if(vec.begin(), vec.end(), [rule_id](const auto & entry) {
        return entry.first == rule_id;
    });
    if (eit == vec.end())
        return false;
    vec.erase(eit);
    return true;
}

std::optional<size_t> DynMessage::add_rule(const std::string & field_name, RuleCallback && callback)
{
    if (_fields.count(field_name) == 0)
        return std::nullopt;
    const size_t id = _next_rule_id++;
    _rules[field_name].emplace_back(id, std::move(callback));
    return id;
}

void DynMessage::reset()
{
    _total_dyn_size = 0;
    _hidden_fields.clear();
}

void DynMessage::_play_rules(const std::string & name, const IMessageField *field)
{
    const auto it = _rules.find(name);
    if (it == _rules.end())
        return;
    for (const auto & [id, callback] : it->second)
        callback(*this, *field);
}

bool DynMessage::field_read_from(const void *buffer, size_t size)
{
    size_t current = 0;
    _total_dyn_size = 0;
    for (const auto & name : this->children_keys())
    {
        IMessageField *field = _fields.at(name);
        if (this->_is_hidden(field))
            continue;
        if (current + field->field_byte_size() > size)
        {
            SIHD_LOG(error,
                     "DynMessage: read error - not enough size {} > {}",
                     current + field->field_byte_size(),
                     size);
            return false;
        }
        if (!field->field_read_from((uint8_t *)buffer + current, field->field_byte_size()))
            return false;
        this->_play_rules(name, field);
        current += field->field_byte_size();
        _total_dyn_size = current;
    }
    return true;
}

bool DynMessage::field_write_to(void *buffer, size_t size)
{
    size_t current = 0;
    for (const auto & name : this->children_keys())
    {
        IMessageField *field = _fields.at(name);
        if (this->_is_hidden(field))
            continue;
        if (current + field->field_byte_size() > size)
        {
            SIHD_LOG(error,
                     "DynMessage: write error - not enough buffer size {} < {}",
                     size,
                     current + field->field_byte_size());
            return false;
        }
        if (!field->field_write_to((uint8_t *)buffer + current, size - current))
            return false;
        current += field->field_byte_size();
    }
    return true;
}

bool DynMessage::hide_field(const std::string & name, bool active)
{
    const auto it = _fields.find(name);
    if (it == _fields.end())
    {
        SIHD_LOG(error, "DynMessage: no such field to {} '{}'", active ? "hide" : "show", name);
        return false;
    }
    if (active)
        return _hidden_fields.insert(it->second).second;
    else
        return _hidden_fields.erase(it->second) > 0;
}

bool DynMessage::_is_hidden(const IMessageField *field) const
{
    return _hidden_fields.count(field) > 0;
}

IMessageField *DynMessage::clone() const
{
    bool error = false;
    DynMessage *cloned = new DynMessage(this->name());
    for (const auto & name : this->children_keys())
    {
        IMessageField *field = _fields.at(name);
        IMessageField *cloned_field = field->clone();
        if (cloned_field != nullptr)
        {
            cloned->add_field(name, cloned_field);
            if (this->_is_hidden(field))
                cloned->hide_field(name, true);
        }
        else
        {
            SIHD_LOG(error, "DynMessage: clone failed for field '{}'", name);
            error = true;
            break;
        }
    }
    if (!error && this->is_finished())
        error = cloned->finish() == false;
    if (error)
    {
        delete cloned;
        cloned = nullptr;
    }
    else
    {
        cloned->_rules = _rules;
        cloned->_next_rule_id = _next_rule_id;
    }
    return cloned;
}

} // namespace sihd::util
