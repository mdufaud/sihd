#include <string>

// clang-format off
#include <sihd/lua/zip/LuaZipApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <luabridge3/LuaBridge/Vector.h>
#include <sihd/lua/checked.hpp>
// clang-format on

#include <sihd/zip/ZipFile.hpp>
#include <sihd/zip/zip.hpp>

namespace sihd::lua
{

using namespace sihd::zip;
using namespace sihd::util;

void LuaZipApi::load_all(Vm & vm)
{
    LuaZipApi::load_base(vm);
}

void LuaZipApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("zip")
        .addFunction(
            "list_entries",
            +[](const std::string & archive) -> std::vector<std::string> { return sihd::zip::list_entries(archive); })
        .addFunction("zip", checked<+[](const std::string & root_dir, const std::string & archive) {
                         return sihd::zip::zip(root_dir, archive);
                     }>)
        .addFunction("unzip", checked<+[](const std::string & archive, const std::string & dest) {
                         return sihd::zip::unzip(archive, dest);
                     }>)
        .beginClass<ZipFile>("ZipFile")
        .addConstructor<void (*)()>()
        .addFunction(
            "open",
            checked<
                +[](ZipFile *self, const std::string & path, luabridge::LuaRef read_only, luabridge::LuaRef strict) {
                    return self->open(path,
                                      read_only.isNil() ? false : static_cast<bool>(read_only),
                                      strict.isNil() ? false : static_cast<bool>(strict));
                }>)
        .addFunction("is_open", &ZipFile::is_open)
        .addFunction("close", checked<+[](ZipFile *self) {
                         return self->close();
                     }>)
        .addFunction("discard", &ZipFile::discard)
        .addFunction("set_password", checked<+[](ZipFile *self, const std::string & password) {
                         return self->set_password(password);
                     }>)
        .addFunction("no_encrypt", &ZipFile::no_encrypt)
        .addFunction("encrypt_in_aes_128", &ZipFile::encrypt_in_aes_128)
        .addFunction("encrypt_in_aes_192", &ZipFile::encrypt_in_aes_192)
        .addFunction("encrypt_in_aes_256", &ZipFile::encrypt_in_aes_256)
        .addFunction(
            "archive_comment",
            +[](ZipFile *self) -> std::string { return std::string(self->archive_comment()); })
        .addFunction("comment_archive", checked<+[](ZipFile *self, const std::string & comment) {
                         return self->comment_archive(comment);
                     }>)
        .addFunction("count_entries", &ZipFile::count_entries)
        .addFunction("count_original_entries", &ZipFile::count_original_entries)
        // load by index (1-based from Lua) or by name
        .addFunction(
            "load_entry",
            checked<+[](ZipFile *self, luabridge::LuaRef key) -> Result<void> {
                if (key.isNumber())
                {
                    const int index = static_cast<int>(key) - 1;
                    if (index < 0)
                        return std::unexpected(Error(ErrorCode::invalid_argument, "zip: entry index is 1-based"));
                    return self->load_entry(static_cast<size_t>(index)).transform([](const ZipFile::ZipEntry &) {});
                }
                return self->load_entry(key.tostring()).transform([](const ZipFile::ZipEntry &) {});
            }>)
        .addFunction("read_next_entry", checked<+[](ZipFile *self) {
                         return self->read_next_entry();
                     }>)
        .addFunction("is_entry_loaded", &ZipFile::is_entry_loaded)
        .addFunction(
            "entry_name",
            +[](ZipFile *self) -> std::string { return std::string(self->entry_name()); })
        .addFunction("entry_index", &ZipFile::entry_index)
        .addFunction(
            "entry_comment",
            +[](ZipFile *self) -> std::string { return std::string(self->entry_comment()); })
        .addFunction("is_entry_directory", &ZipFile::is_entry_directory)
        .addFunction("read_next", checked<+[](ZipFile *self) {
                         return self->read_next();
                     }>)
        .addFunction("get_read_data", &LuaUtilApi::ireader_get_read_data<ZipFile>)
        .addFunction("read_entry", checked<+[](ZipFile *self, luabridge::LuaRef password) {
                         return self->read_entry(password.isNil() ? "" : password.tostring());
                     }>)
        .addFunction("dump_entry_to_fs",
                     checked<+[](ZipFile *self, const std::string & path, luabridge::LuaRef password) {
                         return self->dump_entry_to_fs(path, password.isNil() ? "" : password.tostring());
                     }>)
        .addFunction("remove_entry", checked<+[](ZipFile *self) {
                         return self->remove_entry();
                     }>)
        .addFunction("rename_entry", checked<+[](ZipFile *self, const std::string & name) {
                         return self->rename_entry(name);
                     }>)
        .addFunction("comment_entry", checked<+[](ZipFile *self, const std::string & comment) {
                         return self->comment_entry(comment);
                     }>)
        // add content from filesystem (safe: no dangling buffer)
        .addFunction("add_dir", checked<+[](ZipFile *self, const std::string & name) {
                         return self->add_dir(name);
                     }>)
        .addFunction("add_from_fs", checked<+[](ZipFile *self, const std::string & name, const std::string & path) {
                         return self->add_from_fs(name, path);
                     }>)
        .addFunction("add_dir_from_fs", checked<+[](ZipFile *self, const std::string & name, const std::string & path) {
                         return self->add_dir_from_fs(name, path);
                     }>)
        .addFunction("add_file_from_fs",
                     checked<+[](ZipFile *self, const std::string & name, const std::string & path) {
                         return self->add_file_from_fs(name, path);
                     }>)
        .endClass()
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua
