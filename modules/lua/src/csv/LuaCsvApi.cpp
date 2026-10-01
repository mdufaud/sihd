#include <string>
#include <vector>

// clang-format off
#include <sihd/lua/csv/LuaCsvApi.hpp>
#include <luabridge3/LuaBridge/Vector.h>
// clang-format on

#include <sihd/csv/CsvReader.hpp>
#include <sihd/csv/CsvWriter.hpp>
#include <sihd/csv/utils.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::lua
{

SIHD_LOGGER;

using namespace sihd::csv;

namespace
{

int opt_char(const luabridge::LuaRef & ref, int def)
{
    if (ref.isNil())
        return def;
    if (ref.isString())
    {
        const std::string str = ref;
        return str.empty() ? def : static_cast<unsigned char>(str[0]);
    }
    if (ref.isNumber())
        return static_cast<int>(ref);
    return def;
}

bool opt_bool(const luabridge::LuaRef & ref, bool def)
{
    return ref.isNil() ? def : static_cast<bool>(ref);
}

} // namespace

void LuaCsvApi::load_all(Vm & vm)
{
    LuaCsvApi::load_base(vm);
}

void LuaCsvApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("csv")
        // read whole CSV file -> table of rows (each row a table of string cells), nil on error
        .addFunction(
            "read",
            +[](const std::string & path,
                luabridge::LuaRef remove_header,
                luabridge::LuaRef delimiter,
                luabridge::LuaRef comment,
                lua_State *state) -> luabridge::LuaRef {
                auto data = utils::read_csv(path,
                                            opt_bool(remove_header, false),
                                            opt_char(delimiter, ','),
                                            static_cast<char>(opt_char(comment, '#')));
                if (SIHD_UNEXPECTED_LOG(data))
                    return luabridge::LuaRef(state, luabridge::LuaNil());
                return luabridge::LuaRef(state, *data);
            })
        // parse a CSV string -> table of rows
        .addFunction(
            "from_string",
            +[](const std::string & content,
                luabridge::LuaRef remove_header,
                luabridge::LuaRef delimiter,
                luabridge::LuaRef comment,
                lua_State *state) -> luabridge::LuaRef {
                utils::CsvData data = utils::csv_from_string(content,
                                                             opt_bool(remove_header, false),
                                                             opt_char(delimiter, ','),
                                                             static_cast<char>(opt_char(comment, '#')));
                return luabridge::LuaRef(state, data);
            })
        // write a table of rows (each row a table of string cells) to a CSV file
        .addFunction(
            "write",
            +[](const std::string & path,
                const std::vector<std::vector<std::string>> & rows,
                luabridge::LuaRef delimiter) -> bool {
                CsvWriter writer;
                if (SIHD_UNEXPECTED_LOG(writer.open(path)))
                    return false;
                if (!delimiter.isNil())
                    SIHD_UNEXPECTED_LOG(writer.set_delimiter(opt_char(delimiter, ',')));
                for (const auto & row : rows)
                {
                    if (writer.write_row(row) < 0)
                    {
                        SIHD_LOG(error, "could not write csv row on '{}'", path);
                        return false;
                    }
                }
                return true;
            })
        .addFunction(
            "escape_str",
            +[](const std::string & str) -> std::string { return utils::escape_str(str); })
        // streaming reader
        .beginClass<CsvReader>("CsvReader")
        .addConstructor<void (*)()>()
        .addFunction(
            "open",
            +[](CsvReader *self, const std::string & path) -> bool { return !SIHD_UNEXPECTED_LOG(self->open(path)); })
        .addFunction("is_open", &CsvReader::is_open)
        .addFunction("close", &CsvReader::close)
        .addFunction(
            "set_delimiter",
            +[](CsvReader *self, luabridge::LuaRef c) -> bool {
                return !SIHD_UNEXPECTED_LOG(self->set_delimiter(opt_char(c, ',')));
            })
        .addFunction(
            "set_commentary",
            +[](CsvReader *self, luabridge::LuaRef c) -> bool {
                return !SIHD_UNEXPECTED_LOG(self->set_commentary(opt_char(c, '#')));
            })
        .addFunction("set_timestamp_col", &CsvReader::set_timestamp_col)
        .addFunction("set_timestamp_format", &CsvReader::set_timestamp_format)
        .addFunction(
            "read_next",
            +[](CsvReader *self) -> bool {
                auto next = self->read_next();
                if (SIHD_UNEXPECTED_LOG(next))
                    return false;
                return next.value();
            })
        .addFunction("columns", &CsvReader::columns)
        .endClass()
        // streaming writer
        .beginClass<CsvWriter>("CsvWriter")
        .addConstructor<void (*)()>()
        .addFunction(
            "open",
            +[](CsvWriter *self, const std::string & path, luabridge::LuaRef append) -> bool {
                return !SIHD_UNEXPECTED_LOG(self->open(path, opt_bool(append, false)));
            })
        .addFunction("is_open", &CsvWriter::is_open)
        .addFunction("close", &CsvWriter::close)
        .addFunction(
            "set_delimiter",
            +[](CsvWriter *self, luabridge::LuaRef c) -> bool {
                return !SIHD_UNEXPECTED_LOG(self->set_delimiter(opt_char(c, ',')));
            })
        .addFunction(
            "set_commentary",
            +[](CsvWriter *self, luabridge::LuaRef c) -> bool {
                return !SIHD_UNEXPECTED_LOG(self->set_commentary(opt_char(c, '#')));
            })
        .addFunction(
            "write_row",
            +[](CsvWriter *self, const std::vector<std::string> & values) -> long {
                return static_cast<long>(self->write_row(values));
            })
        .addFunction(
            "write_commentary",
            +[](CsvWriter *self, const std::string & commentary) -> long {
                return static_cast<long>(self->write_commentary(commentary));
            })
        .addFunction(
            "new_row",
            +[](CsvWriter *self) -> bool { return !SIHD_UNEXPECTED_LOG(self->new_row()); })
        .addFunction("current_row", &CsvWriter::current_row)
        .addFunction("current_col", &CsvWriter::current_col)
        .addFunction("max_col", &CsvWriter::max_col)
        .endClass()
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua
