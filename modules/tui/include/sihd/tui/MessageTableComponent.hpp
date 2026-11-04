#ifndef __SIHD_TUI_MESSAGETABLECOMPONENT_HPP__
#define __SIHD_TUI_MESSAGETABLECOMPONENT_HPP__

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include <sihd/core/Channel.hpp>
#include <sihd/util/Message.hpp>
#include <sihd/util/MessageField.hpp>

namespace sihd::tui
{

using FieldFormatter = std::function<std::string(const sihd::util::MessageField &)>;

struct MessageTableOptions
{
        std::vector<std::string> fields;
        std::map<std::string, std::string> titles;
        std::map<std::string, FieldFormatter> formatters;
        size_t sort_column = 0;
        bool sort_descending = false;
};

// a char field as text up to its first NUL, a ubyte array as hex bytes joined by ':', a value otherwise
std::string field_str(const sihd::util::MessageField & field);

// `records` must outlive the component, `schema` is cloned
ftxui::Component MessageTableComponent(const sihd::core::Channel & records,
                                       const sihd::util::Message & schema,
                                       MessageTableOptions && options);

} // namespace sihd::tui

#endif
