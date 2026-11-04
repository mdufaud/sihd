#include <algorithm>
#include <memory>

#include <sihd/tui/MessageTableComponent.hpp>
#include <sihd/tui/TableComponent.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Value.hpp>
#include <sihd/util/str.hpp>

namespace sihd::tui
{

using namespace sihd::util;
using namespace sihd::core;

namespace
{

std::vector<std::string> row_of(const Message & record,
                                const std::vector<std::string> & fields,
                                const std::map<std::string, FieldFormatter> & formatters)
{
    std::vector<std::string> row;
    for (const std::string & name : fields)
    {
        const MessageField *field = record.get_child<MessageField>(name);
        const auto formatter = formatters.find(name);
        if (field == nullptr)
            row.emplace_back();
        else
            row.push_back(formatter == formatters.end() ? field_str(*field) : formatter->second(*field));
    }
    return row;
}

} // namespace

std::string field_str(const MessageField & field)
{
    const IArray *array = field.array();
    if (array == nullptr)
        return {};
    const uint8_t *buf = array->buf();
    const Type type = field.field_type();
    if (type == TYPE_CHAR)
    {
        const char *chars = reinterpret_cast<const char *>(buf);
        return std::string(chars, std::find(chars, chars + array->byte_size(), '\0'));
    }
    if (type == TYPE_UBYTE && field.field_size() > 1)
        return str::hexdump(buf, array->byte_size(), ':');
    std::string text;
    for (size_t i = 0; i < field.field_size(); ++i)
        text += (i == 0 ? "" : " ") + Value(buf + i * type::size(type), type).str();
    return text;
}

ftxui::Component MessageTableComponent(const Channel & records, const Message & schema, MessageTableOptions && options)
{
    std::shared_ptr<Message> record(static_cast<Message *>(schema.clone()));
    std::vector<std::string> fields = options.fields;
    if (fields.empty())
        fields = schema.children_keys();

    TableOptions table;
    for (const std::string & name : fields)
    {
        const auto title = options.titles.find(name);
        table.columns.push_back(title == options.titles.end() ? name : title->second);
    }
    table.sort_column = options.sort_column;
    table.sort_descending = options.sort_descending;

    table.rows = [&records, record, fields, formatters = std::move(options.formatters)] {
        TableRows rows;
        ArrUByte bytes;
        if (record == nullptr || records.copy_resized_to(bytes) == false)
            return rows;
        record->for_each_record(bytes, [&](const Message & read) { rows.push_back(row_of(read, fields, formatters)); });
        return rows;
    };
    return TableComponent(std::move(table));
}

} // namespace sihd::tui
