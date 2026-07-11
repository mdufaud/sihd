#include <string>

// clang-format off
#include <sihd/lua/zip/LuaZipApi.hpp>
#include <sihd/lua/util/LuaUtilApi.hpp>
#include <luabridge3/LuaBridge/Vector.h>
// clang-format on

#include <sihd/zip/ZipFile.hpp>
#include <sihd/zip/zip.hpp>

namespace sihd::lua
{

using namespace sihd::zip;

void LuaZipApi::load_all(Vm & vm)
{
    LuaZipApi::load_base(vm);
}

void LuaZipApi::load_base(Vm & vm)
{
    luabridge::getGlobalNamespace(vm.lua_state())
        .beginNamespace("sihd")
        .beginNamespace("zip")
        // whole-archive helpers (highest value, simplest)
        .addFunction("list_entries", +[](const std::string & archive) -> std::vector<std::string> {
            return sihd::zip::list_entries(archive);
        })
        .addFunction("zip", +[](const std::string & root_dir, const std::string & archive) -> bool {
            return sihd::zip::zip(root_dir, archive);
        })
        .addFunction("unzip", +[](const std::string & archive, const std::string & dest) -> bool {
            return sihd::zip::unzip(archive, dest);
        })
        // fuller control
        .beginClass<ZipFile>("ZipFile")
        .addConstructor<void (*)()>()
        .addFunction("open",
                     +[](ZipFile *self, const std::string & path, luabridge::LuaRef read_only, luabridge::LuaRef strict)
                         -> bool {
                         return self->open(path,
                                           read_only.isNil() ? false : static_cast<bool>(read_only),
                                           strict.isNil() ? false : static_cast<bool>(strict));
                     })
        .addFunction("is_open", &ZipFile::is_open)
        .addFunction("close", &ZipFile::close)
        .addFunction("discard", &ZipFile::discard)
        .addFunction("set_password",
                     +[](ZipFile *self, const std::string & password) -> bool { return self->set_password(password); })
        .addFunction("no_encrypt", &ZipFile::no_encrypt)
        .addFunction("encrypt_in_aes_128", &ZipFile::encrypt_in_aes_128)
        .addFunction("encrypt_in_aes_192", &ZipFile::encrypt_in_aes_192)
        .addFunction("encrypt_in_aes_256", &ZipFile::encrypt_in_aes_256)
        .addFunction("archive_comment",
                     +[](ZipFile *self) -> std::string { return std::string(self->archive_comment()); })
        .addFunction("comment_archive",
                     +[](ZipFile *self, const std::string & comment) -> bool { return self->comment_archive(comment); })
        .addFunction("count_entries", &ZipFile::count_entries)
        .addFunction("count_original_entries", &ZipFile::count_original_entries)
        // load by index (1-based from Lua) or by name
        .addFunction("load_entry",
                     +[](ZipFile *self, luabridge::LuaRef key) -> bool {
                         if (key.isNumber())
                             return self->load_entry(static_cast<size_t>(static_cast<int>(key) - 1));
                         return self->load_entry(std::string(key.tostring()));
                     })
        .addFunction("read_next_entry", &ZipFile::read_next_entry)
        .addFunction("is_entry_loaded", &ZipFile::is_entry_loaded)
        .addFunction("entry_name", +[](ZipFile *self) -> std::string { return std::string(self->entry_name()); })
        .addFunction("entry_index", &ZipFile::entry_index)
        .addFunction("entry_comment", +[](ZipFile *self) -> std::string { return std::string(self->entry_comment()); })
        .addFunction("is_entry_directory", &ZipFile::is_entry_directory)
        // read loaded entry content
        .addFunction("read_next", &ZipFile::read_next)
        .addFunction("get_read_data", &LuaUtilApi::ireader_get_read_data<ZipFile>)
        .addFunction("read_entry",
                     +[](ZipFile *self, luabridge::LuaRef password) -> long {
                         return static_cast<long>(self->read_entry(password.isNil() ? "" : password.tostring()));
                     })
        .addFunction("dump_entry_to_fs",
                     +[](ZipFile *self, const std::string & path, luabridge::LuaRef password) -> bool {
                         return self->dump_entry_to_fs(path, password.isNil() ? "" : password.tostring());
                     })
        // modify entries
        .addFunction("remove_entry", &ZipFile::remove_entry)
        .addFunction("rename_entry",
                     +[](ZipFile *self, const std::string & name) -> bool { return self->rename_entry(name); })
        .addFunction("comment_entry",
                     +[](ZipFile *self, const std::string & comment) -> bool { return self->comment_entry(comment); })
        // add content from filesystem (safe: no dangling buffer)
        .addFunction("add_dir", +[](ZipFile *self, const std::string & name) -> bool { return self->add_dir(name); })
        .addFunction("add_from_fs",
                     +[](ZipFile *self, const std::string & name, const std::string & path) -> bool {
                         return self->add_from_fs(name, path);
                     })
        .addFunction("add_dir_from_fs",
                     +[](ZipFile *self, const std::string & name, const std::string & path) -> bool {
                         return self->add_dir_from_fs(name, path);
                     })
        .addFunction("add_file_from_fs",
                     +[](ZipFile *self, const std::string & name, const std::string & path) -> bool {
                         return self->add_file_from_fs(name, path);
                     })
        .endClass()
        .endNamespace()
        .endNamespace();
}

} // namespace sihd::lua
