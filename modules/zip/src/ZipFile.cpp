#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wmodule-import-in-extern-c"
#endif
#include <zip.h>
#ifdef __clang__
# pragma clang diagnostic pop
#endif

#include <cstring>
#include <optional>
#include <utility>

#include <fmt/core.h>

#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/zip/ZipFile.hpp>
#include <sihd/zip/zip.hpp>

#define ZIP_ENTRY_NAME_OR_INDEX(entry) (entry.name != nullptr ? entry.name : std::to_string(entry.index))

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using namespace sihd::sys;

namespace sihd::zip
{

SIHD_NEW_LOGGER("sihd::zip");

namespace
{

ErrorCode zip_error_code(int ze)
{
    switch (ze)
    {
        case ZIP_ER_EXISTS:
            return already_exists;
        case ZIP_ER_NOENT:
        case ZIP_ER_NOZIP:
            return not_found;
        case ZIP_ER_OPEN:
            return io_error;
        case ZIP_ER_RDONLY:
            return permission_denied;
        case ZIP_ER_INCONS:
        case ZIP_ER_CRC:
            return io_error;
        case ZIP_ER_MEMORY:
            return out_of_memory;
        default:
            return io_error;
    }
}

std::string get_error(int code)
{
    zip_error_t ziperror;
    zip_error_init_with_code(&ziperror, code);
    std::string error_str = zip_error_strerror(&ziperror);
    zip_error_fini(&ziperror);
    return error_str;
}

std::string get_error(zip_t *ptr)
{
    return zip_strerror(ptr);
}

Error make_error(int ze, std::string && message)
{
    return Error(zip_error_code(ze), "{}: {} ({})", std::move(message), get_error(ze), ze);
}

template <typename... Args>
    requires(sizeof...(Args) != 0)
Error make_error(int ze, fmt::format_string<Args...> format, Args &&...args)
{
    return make_error(ze, fmt::format(format, std::forward<Args>(args)...));
}

Error make_error(zip_t *ptr, std::string && message)
{
    const int ze = zip_error_code_zip(zip_get_error(ptr));
    return Error(zip_error_code(ze), "{}: {} ({})", std::move(message), get_error(ptr), ze);
}

template <typename... Args>
    requires(sizeof...(Args) != 0)
Error make_error(zip_t *ptr, fmt::format_string<Args...> format, Args &&...args)
{
    return make_error(ptr, fmt::format(format, std::forward<Args>(args)...));
}

Error make_error(zip_file *file_ptr, std::string && message)
{
    const int ze = zip_error_code_zip(zip_file_get_error(file_ptr));
    return Error(zip_error_code(ze), "{}: {} ({})", std::move(message), zip_file_strerror(file_ptr), ze);
}

template <typename... Args>
    requires(sizeof...(Args) != 0)
Error make_error(zip_file *file_ptr, fmt::format_string<Args...> format, Args &&...args)
{
    return make_error(file_ptr, fmt::format(format, std::forward<Args>(args)...));
}

Error not_open_error()
{
    return Error(not_initialized, "no zip file open");
}

Error no_entry_error()
{
    return Error(not_initialized, "no entry loaded");
}

std::expected<void, Error> add_source(zip_t *zip_ptr, std::string_view name, zip_source_t *source)
{
    if (zip_ptr == nullptr)
    {
        zip_source_free(source);
        return std::unexpected(not_open_error());
    }
    if (zip_file_add(zip_ptr, name.data(), source, ZIP_FL_ENC_UTF_8) < 0)
    {
        zip_source_free(source);
        return std::unexpected(make_error(zip_ptr, "could not add file '{}'", name));
    }
    return {};
}

void save_entry(struct zip_stat *zip_stat, ZipFile::ZipEntry & zip_entry)
{
    if (zip_stat->valid & ZIP_STAT_NAME)
        zip_entry.name = zip_stat->name;
    if (zip_stat->valid & ZIP_STAT_INDEX)
        zip_entry.index = zip_stat->index;
    if (zip_stat->valid & ZIP_STAT_SIZE)
        zip_entry.size = zip_stat->size;
    if (zip_stat->valid & ZIP_STAT_COMP_SIZE)
        zip_entry.compressed_size = zip_stat->comp_size;
    if (zip_stat->valid & ZIP_STAT_MTIME)
        zip_entry.modification_time = sihd::util::Timestamp(zip_stat->mtime);
    if (zip_stat->valid & ZIP_STAT_CRC)
        zip_entry.crc = zip_stat->crc;
}

void close_zip_file_and_null(zip_file **file_ptr)
{
    if (file_ptr != nullptr && *file_ptr != nullptr)
    {
        zip_fclose(*file_ptr);
        *file_ptr = nullptr;
    }
}

} // namespace

struct ZipFile::ZipHandle
{
        struct zip *handle_ptr;
        struct zip_file *file_handle_ptr;
};

ZipFile::ZipFile(): _zip_handle(std::make_unique<ZipHandle>())
{
    this->no_encrypt();
}

ZipFile::ZipFile(std::string_view path, bool read_only, bool do_strict_checks): ZipFile()
{
    SIHD_UNEXPECTED_LOG(this->open(path, read_only, do_strict_checks));
}

ZipFile::~ZipFile()
{
    this->discard();
}

std::expected<void, Error> ZipFile::set_password(std::string_view password)
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());
    if (zip_set_default_password(_zip_handle->handle_ptr, password.data()) < 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr, "could not set password"));
    return {};
}

std::expected<void, Error> ZipFile::set_buffer_size(size_t size)
{
    if (size == 0)
        return std::unexpected(Error(invalid_argument, "cannot set buffer to 0"));
    _buf.reserve(size + 1);
    return {};
}

std::expected<void, Error> ZipFile::set_aes_encryption(int aes)
{
    switch (aes)
    {
        case 0:
            this->no_encrypt();
            break;
        case 128:
            this->encrypt_in_aes_128();
            break;
        case 192:
            this->encrypt_in_aes_192();
            break;
        case 256:
            this->encrypt_in_aes_256();
            break;
        default:
            return std::unexpected(Error(invalid_argument, "no such encryption for AES: {}", aes));
    }
    return {};
}

std::expected<void, Error> ZipFile::open(std::string_view path, bool read_only, bool do_strict_checks)
{
    // a failing close must not prevent opening another archive
    SIHD_UNEXPECTED_LOG(this->close());

    int flags = ZIP_CREATE;
    if (read_only)
        flags |= ZIP_RDONLY;
    if (do_strict_checks)
        flags |= ZIP_CHECKCONS;

    int ze = 0;
    _zip_handle->handle_ptr = zip_open(path.data(), flags, &ze);
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(make_error(ze, "could not open zip '{}'", path));

    _current_zip_entry = ZipEntry {};
    _zip_handle->file_handle_ptr = nullptr;
    _buf.clear();
    return {};
}

bool ZipFile::is_open() const
{
    return _zip_handle->handle_ptr != nullptr;
}

void ZipFile::discard()
{
    if (_zip_handle->handle_ptr != nullptr)
    {
        close_zip_file_and_null(&_zip_handle->file_handle_ptr);
        zip_discard(_zip_handle->handle_ptr);
        _zip_handle->handle_ptr = nullptr;
    }
    _current_zip_entry = ZipEntry {};
    _buf.clear();
}

std::expected<void, Error> ZipFile::close()
{
    std::optional<Error> error;
    if (_zip_handle->handle_ptr != nullptr)
    {
        close_zip_file_and_null(&_zip_handle->file_handle_ptr);
        if (zip_close(_zip_handle->handle_ptr) != 0)
            error = make_error(_zip_handle->handle_ptr, "could not close zip file");
        _zip_handle->handle_ptr = nullptr;
    }
    _current_zip_entry = ZipEntry {};
    _buf.clear();
    if (error)
        return std::unexpected(std::move(*error));
    return {};
}

std::string_view ZipFile::archive_comment() const
{
    if (_zip_handle->handle_ptr == nullptr)
        return "";
    int comment_length;
    const char *archive_comment = zip_get_archive_comment(_zip_handle->handle_ptr, &comment_length, 0);
    return archive_comment != nullptr ? std::string_view(archive_comment, comment_length) : "";
}

std::expected<void, Error> ZipFile::comment_archive(std::string_view comment)
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());
    if (zip_set_archive_comment(_zip_handle->handle_ptr, comment.data(), comment.size()) != 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr, "could not write zip archive commentary"));
    return {};
}

std::expected<void, Error> ZipFile::set_archive_readonly(bool active)
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());
    if (zip_set_archive_flag(_zip_handle->handle_ptr, ZIP_AFL_RDONLY, (int)active) != 0)
        return std::unexpected(
            make_error(_zip_handle->handle_ptr, "could not set archive {}", active ? "read-only" : "non read-only"));
    return {};
}

void ZipFile::no_encrypt()
{
    _encryption_method = ZIP_EM_NONE;
}

void ZipFile::encrypt_in_aes_128()
{
    _encryption_method = ZIP_EM_AES_128;
}

void ZipFile::encrypt_in_aes_192()
{
    _encryption_method = ZIP_EM_AES_192;
}

void ZipFile::encrypt_in_aes_256()
{
    _encryption_method = ZIP_EM_AES_256;
}

std::expected<void, Error> ZipFile::encrypt_all(std::string_view password)
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());

    const size_t total_entries = this->count_entries();
    size_t idx = 0;
    while (idx < total_entries)
    {
        auto loaded = this->load_entry(idx);
        SIHD_UNEXPECTED_RETURN(loaded);
        auto encrypted = this->encrypt_entry(password);
        SIHD_UNEXPECTED_RETURN(encrypted);
        ++idx;
    }
    return {};
}

ssize_t ZipFile::count_original_entries() const
{
    if (_zip_handle->handle_ptr == nullptr)
        return -1;
    return zip_get_num_entries(_zip_handle->handle_ptr, ZIP_FL_UNCHANGED);
}

ssize_t ZipFile::count_entries() const
{
    if (_zip_handle->handle_ptr == nullptr)
        return -1;
    return zip_get_num_entries(_zip_handle->handle_ptr, 0);
}

std::expected<ZipFile::ZipEntry, Error> ZipFile::load_entry(size_t idx)
{
    _current_zip_entry = ZipEntry {};
    close_zip_file_and_null(&_zip_handle->file_handle_ptr);

    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());

    zip_stat_t zip_entry;
    zip_stat_init(&zip_entry);
    if (zip_stat_index(_zip_handle->handle_ptr, idx, 0, &zip_entry) != 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr, "could not read entry '{}'", idx));
    save_entry(&zip_entry, _current_zip_entry);
    return _current_zip_entry;
}

std::expected<ZipFile::ZipEntry, Error> ZipFile::load_entry(std::string_view name)
{
    _current_zip_entry = ZipEntry {};
    close_zip_file_and_null(&_zip_handle->file_handle_ptr);

    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());

    zip_stat_t zip_entry;
    zip_stat_init(&zip_entry);
    if (zip_stat(_zip_handle->handle_ptr, name.data(), 0, &zip_entry) != 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr, "could not read entry '{}'", name));
    save_entry(&zip_entry, _current_zip_entry);
    return _current_zip_entry;
}

bool ZipFile::is_entry_loaded() const
{
    return _zip_handle->handle_ptr != nullptr && _current_zip_entry.index >= 0;
}

std::expected<bool, Error> ZipFile::read_next_entry()
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());
    const ssize_t total_entries = this->count_original_entries();
    if (_current_zip_entry.index + 1 >= total_entries)
        return false;
    auto entry = this->load_entry(_current_zip_entry.index + 1);
    SIHD_UNEXPECTED_RETURN(entry);
    return true;
}

std::string_view ZipFile::entry_name() const
{
    return _current_zip_entry.name == nullptr ? "" : _current_zip_entry.name;
}

ssize_t ZipFile::entry_index() const
{
    return _current_zip_entry.index;
}

const ZipFile::ZipEntry & ZipFile::current_entry() const
{
    return _current_zip_entry;
}

std::string_view ZipFile::entry_comment() const
{
    if (this->is_entry_loaded() == false)
        return "";
    zip_uint32_t comment_length;
    const char *comment = zip_file_get_comment(_zip_handle->handle_ptr, _current_zip_entry.index, &comment_length, 0);
    return comment == nullptr ? "" : std::string_view(comment, comment_length);
}

bool ZipFile::is_entry_directory() const
{
    if (_zip_handle->handle_ptr == nullptr || _current_zip_entry.name == nullptr)
        return false;
    size_t len = strlen(_current_zip_entry.name);
    if (len == 0)
        return false;
    return _current_zip_entry.name[len - 1] == '/';
}

std::expected<bool, Error> ZipFile::read_next()
{
    auto size_read = this->read_entry();
    SIHD_UNEXPECTED_RETURN(size_read);
    return *size_read > 0;
}

bool ZipFile::get_read_data(sihd::util::ArrCharView & view) const
{
    if (this->is_entry_loaded() == false)
        return false;
    view = {_buf.data(), _buf.size()};
    return true;
}

std::expected<ssize_t, Error> ZipFile::read_entry(std::string_view password)
{
    _buf.clear();
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());

    if (_zip_handle->file_handle_ptr == nullptr)
    {
        if (password.empty() == false)
            _zip_handle->file_handle_ptr = zip_fopen_index_encrypted(_zip_handle->handle_ptr,
                                                                     _current_zip_entry.index,
                                                                     0,
                                                                     password.data());
        else
            _zip_handle->file_handle_ptr = zip_fopen_index(_zip_handle->handle_ptr, _current_zip_entry.index, 0);
        if (_zip_handle->file_handle_ptr == nullptr)
            return std::unexpected(
                make_error(_zip_handle->handle_ptr, "could not open entry '{}'", _current_zip_entry.name));
    }

    if (_buf.capacity() == 0)
    {
        auto resized = this->set_buffer_size(4096);
        SIHD_UNEXPECTED_RETURN(resized);
    }

    _buf.resize(_buf.capacity());
    // -1 to account for the \0
    ssize_t ret = zip_fread(_zip_handle->file_handle_ptr, _buf.data(), _buf.capacity() - 1);
    if (ret < 0)
    {
        auto error = make_error(_zip_handle->file_handle_ptr, "could not read entry '{}'", _current_zip_entry.name);
        _buf.clear();
        close_zip_file_and_null(&_zip_handle->file_handle_ptr);
        return std::unexpected(std::move(error));
    }
    _buf.resize((size_t)ret);
    if (ret > 0)
        _buf[ret] = 0;
    if (ret == 0)
        close_zip_file_and_null(&_zip_handle->file_handle_ptr);
    return ret;
}

std::expected<void, Error> ZipFile::unchange_entry()
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_unchange(_zip_handle->handle_ptr, _current_zip_entry.index) < 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr,
                                          "could not remove changes to entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    return {};
}

std::expected<void, Error> ZipFile::remove_entry()
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_delete(_zip_handle->handle_ptr, _current_zip_entry.index) < 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr,
                                          "could not remove entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    return {};
}

std::expected<void, Error> ZipFile::rename_entry(std::string_view new_name)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_file_rename(_zip_handle->handle_ptr, _current_zip_entry.index, new_name.data(), 0) < 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr,
                                          "could not rename entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    auto reloaded = this->load_entry(_current_zip_entry.index);
    SIHD_UNEXPECTED_RETURN_CTX(reloaded,
                               "renaming entry '{}' to '{}'",
                               ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                               new_name);
    return {};
}

std::expected<void, Error> ZipFile::modify_entry_time(sihd::util::Timestamp new_timestamp)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_file_set_mtime(_zip_handle->handle_ptr, _current_zip_entry.index, new_timestamp.seconds(), 0) < 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr,
                                          "could not modify entry time '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    auto reloaded_2 = this->load_entry(_current_zip_entry.index);
    SIHD_UNEXPECTED_RETURN(reloaded_2);
    return {};
}

std::expected<void, Error> ZipFile::comment_entry(std::string_view comment)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_file_set_comment(_zip_handle->handle_ptr, _current_zip_entry.index, comment.data(), comment.size(), 0) < 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr,
                                          "could not comment entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    return {};
}

std::expected<void, Error> ZipFile::encrypt_entry(std::string_view password)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_file_set_encryption(_zip_handle->handle_ptr, _current_zip_entry.index, _encryption_method, password.data())
        < 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr,
                                          "failed to set encryption on index '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    return {};
}

std::expected<void, Error> ZipFile::replace_entry(sihd::util::ArrCharView view)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());

    close_zip_file_and_null(&_zip_handle->file_handle_ptr);

    zip_source_t *source = zip_source_buffer(_zip_handle->handle_ptr, view.data(), view.byte_size(), 0);
    if (source == nullptr)
        return std::unexpected(make_error(_zip_handle->handle_ptr,
                                          "could not create source buffer for entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));

    if (zip_file_replace(_zip_handle->handle_ptr, _current_zip_entry.index, source, 0) != 0)
    {
        zip_source_free(source);
        return std::unexpected(make_error(_zip_handle->handle_ptr,
                                          "could not replace entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    }
    return {};
}

std::expected<void, Error> ZipFile::add_dir(std::string_view name)
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());
    if (zip_dir_add(_zip_handle->handle_ptr, name.data(), 0) < 0)
        return std::unexpected(make_error(_zip_handle->handle_ptr, "could not add directory '{}'", name));
    return {};
}

std::expected<void, Error> ZipFile::add_file(std::string_view name, sihd::util::ArrCharView view)
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());
    zip_source_t *source = zip_source_buffer(_zip_handle->handle_ptr, view.data(), view.byte_size(), 0);
    if (source == nullptr)
        return std::unexpected(
            make_error(_zip_handle->handle_ptr, "could not create source buffer for entry '{}'", name));
    return add_source(_zip_handle->handle_ptr, name, source);
}

std::expected<void, Error> ZipFile::add_from_fs(std::string_view name, std::string_view path)
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());

    if (fs::is_dir(path))
        return this->add_dir_from_fs(name, path);
    if (fs::is_file(path))
        return this->add_file_from_fs(name, path);

    return std::unexpected(Error(not_found, "no such file or directory '{}'", path));
}

std::expected<void, Error> ZipFile::add_dir_from_fs(std::string_view name, std::string_view path)
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());
    if (auto added = this->add_dir(name); !added)
        return added;
    std::vector<std::string> children = fs::children(path);
    for (std::string_view child : children)
    {
        // fs::children appends the host separator to directory entries: strip it
        std::string child_name(child);
        if (!child_name.empty() && child_name.back() == fs::sep())
            child_name.pop_back();
        // archive entry names always use '/', regardless of the host fs separator
        std::string entry_name = std::string(name) + "/" + child_name;
        if (auto added = this->add_from_fs(entry_name, fs::combine(path, child_name)); !added)
            return added;
    }
    return {};
}

std::expected<void, Error> ZipFile::add_file_from_fs(std::string_view name, std::string_view path)
{
    if (_zip_handle->handle_ptr == nullptr)
        return std::unexpected(not_open_error());
    zip_source_t *source = zip_source_file(_zip_handle->handle_ptr, path.data(), 0, 0);
    if (source == nullptr)
        return std::unexpected(
            make_error(_zip_handle->handle_ptr, "could not create source file for entry '{}'", name));
    return add_source(_zip_handle->handle_ptr, name, source);
}

std::expected<void, Error> ZipFile::dump_entry_to_fs(std::string_view path, std::string_view password)
{
    if (_zip_handle->handle_ptr == nullptr || !this->is_entry_loaded())
        return std::unexpected(no_entry_error());

    if (this->is_entry_directory())
    {
        auto res = fs::make_directory(path.data(), 0750);
        SIHD_UNEXPECTED_RETURN(res);
        return {};
    }

    File file(path, "w");

    if (!file.is_open())
    {
        // File::open already logged the errno detail: reading it back may yield a clobbered value
        return std::unexpected(Error(io_error, "could not write entry '{}' to '{}'", _current_zip_entry.name, path));
    }

    while (true)
    {
        auto read = this->read_entry(password);
        SIHD_UNEXPECTED_RETURN(read);
        if (*read == 0)
            break;
        auto wrote = file.write(_buf.data(), (size_t)*read);
        // a failed write leaves the entry half-read: drop the handle so a retry restarts it
        if (!wrote)
            close_zip_file_and_null(&_zip_handle->file_handle_ptr);
        SIHD_UNEXPECTED_RETURN_CTX(wrote,
                                   "writing {} bytes of entry '{}' to '{}'",
                                   *read,
                                   _current_zip_entry.name,
                                   path);
        if (*wrote == 0)
            break;
    }
    return {};
}

} // namespace sihd::zip
