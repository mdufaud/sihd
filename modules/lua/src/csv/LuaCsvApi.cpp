#include <string>
#include <vector>

// clang-format off
#include <sihd/lua/csv/LuaCsvApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <luabridge3/LuaBridge/Vector.h>
#include <sihd/lua/checked.hpp>
// clang-format on

#include <sihd/core/ACoreObject.hpp>
#include <sihd/csv/CsvReader.hpp>
#include <sihd/csv/CsvRecorder.hpp>
#include <sihd/csv/CsvWriter.hpp>
#include <sihd/csv/utils.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/SmartNodePtr.hpp>

namespace sihd::lua
{

using namespace sihd::csv;
using namespace sihd::util;
using sihd::core::ACoreObject;

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
        .addFunction("read",
                     checked<+[](const std::string & path,
                                 luabridge::LuaRef remove_header,
                                 luabridge::LuaRef delimiter,
                                 luabridge::LuaRef comment) {
                         return utils::read_csv(path,
                                                opt_bool(remove_header, false),
                                                opt_char(delimiter, ','),
                                                static_cast<char>(opt_char(comment, '#')));
                     }>)
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
        .addFunction(
            "write",
            checked<+[](const std::string & path,
                        const std::vector<std::vector<std::string>> & rows,
                        luabridge::LuaRef delimiter) -> Result<void> {
                CsvWriter writer;
                if (!delimiter.isNil())
                {
                    auto delimited = writer.set_delimiter(opt_char(delimiter, ','));
                    SIHD_UNEXPECTED_RETURN(delimited);
                }
                auto opened = writer.open(path);
                SIHD_UNEXPECTED_RETURN(opened);
                for (const auto & row : rows)
                {
                    if (writer.write_row(row) < 0)
                        return std::unexpected(Error(ErrorCode::io_error, "could not write csv row on '{}'", path));
                }
                return {};
            }>)
        .addFunction(
            "escape_str",
            +[](const std::string & str) -> std::string { return utils::escape_str(str); })
        .beginClass<CsvReader>("CsvReader")
        .addConstructor<void (*)()>()
        .addFunction("open", checked<+[](CsvReader *self, const std::string & path) {
                         return self->open(path);
                     }>)
        .addFunction("is_open", &CsvReader::is_open)
        .addFunction("close", &CsvReader::close)
        .addFunction("set_delimiter", checked<+[](CsvReader *self, luabridge::LuaRef c) {
                         return self->set_delimiter(opt_char(c, ','));
                     }>)
        .addFunction("set_commentary", checked<+[](CsvReader *self, luabridge::LuaRef c) {
                         return self->set_commentary(opt_char(c, '#'));
                     }>)
        .addFunction("set_timestamp_col", &CsvReader::set_timestamp_col)
        .addFunction("set_timestamp_format", &CsvReader::set_timestamp_format)
        .addFunction("read_next", checked<+[](CsvReader *self) {
                         return self->read_next();
                     }>)
        .addFunction("columns", &CsvReader::columns)
        .endClass()
        .beginClass<CsvWriter>("CsvWriter")
        .addConstructor<void (*)()>()
        .addFunction("open", checked<+[](CsvWriter *self, const std::string & path, luabridge::LuaRef append) {
                         return self->open(path, opt_bool(append, false));
                     }>)
        .addFunction("is_open", &CsvWriter::is_open)
        .addFunction("close", &CsvWriter::close)
        .addFunction("set_delimiter", checked<+[](CsvWriter *self, luabridge::LuaRef c) {
                         return self->set_delimiter(opt_char(c, ','));
                     }>)
        .addFunction("set_commentary", checked<+[](CsvWriter *self, luabridge::LuaRef c) {
                         return self->set_commentary(opt_char(c, '#'));
                     }>)
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
        .addFunction("new_row", checked<+[](CsvWriter *self) {
                         return self->new_row();
                     }>)
        .addFunction("current_row", &CsvWriter::current_row)
        .addFunction("current_col", &CsvWriter::current_col)
        .addFunction("max_col", &CsvWriter::max_col)
        .endClass()
        // derives from core ACoreObject: core must be loaded first
        .deriveClass<CsvRecorder, ACoreObject>("CsvRecorder")
        .addConstructorFrom<SmartNodePtr<CsvRecorder>, void(const std::string &, Node *)>()
        .addFunction("set_path", &CsvRecorder::set_path)
        .endClass()
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua
