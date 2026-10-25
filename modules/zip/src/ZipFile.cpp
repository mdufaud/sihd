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
#include <sihd/util/Result.hpp>
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

Result<void> add_source(zip_t *zip_ptr, const std::string & name, zip_source_t *source)
{
    if (zip_ptr == nullptr)
    {
        zip_source_free(source);
        return std::unexpected(not_open_error());
    }
    if (zip_file_add(zip_ptr, name.c_str(), source, ZIP_FL_ENC_UTF_8) < 0)
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
        zip_entry.modification_time = Timestamp(zip_stat->mtime);
    if (zip_stat->valid & ZIP_STAT_CRC)
        zip_entry.crc = zip_stat->crc;
}

struct ArchiveFree
{
        void operator()(zip_t *archive) const { zip_discard(archive); }
};

struct FileFree
{
        void operator()(zip_file_t *file) const { zip_fclose(file); }
};

} // namespace

struct ZipFile::ZipHandle
{
        std::unique_ptr<zip_t, ArchiveFree> archive;
        std::unique_ptr<zip_file_t, FileFree> file;
};

ZipFile::ZipFile(): _zip_handle(std::make_unique<ZipHandle>())
{
    this->no_encrypt();
}

ZipFile::ZipFile(const std::string & path, bool read_only, bool do_strict_checks): ZipFile()
{
    SIHD_UNEXPECTED_LOG(this->open(path, read_only, do_strict_checks));
}

ZipFile::~ZipFile()
{
    this->discard();
}

Result<void> ZipFile::set_password(const std::string & password)
{
    if (_zip_handle->archive == nullptr)
        return std::unexpected(not_open_error());
    if (zip_set_default_password(_zip_handle->archive.get(), password.c_str()) < 0)
        return std::unexpected(make_error(_zip_handle->archive.get(), "could not set password"));
    return {};
}

Result<void> ZipFile::set_buffer_size(size_t size)
{
    if (size == 0)
        return std::unexpected(Error(invalid_argument, "cannot set buffer to 0"));
    _buf.reserve(size + 1);
    return {};
}

Result<void> ZipFile::set_aes_encryption(int aes)
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

Result<void> ZipFile::open(const std::string & path, bool read_only, bool do_strict_checks)
{
    // a failing close must not prevent opening another archive
    SIHD_UNEXPECTED_LOG(this->close());

    int flags = ZIP_CREATE;
    if (read_only)
        flags |= ZIP_RDONLY;
    if (do_strict_checks)
        flags |= ZIP_CHECKCONS;

    int ze = 0;
    _zip_handle->archive.reset(zip_open(path.c_str(), flags, &ze));
    if (_zip_handle->archive == nullptr)
        return std::unexpected(make_error(ze, "could not open zip '{}'", path));

    _current_zip_entry = ZipEntry {};
    _zip_handle->file.reset();
    _buf.clear();
    return {};
}

bool ZipFile::is_open() const
{
    return _zip_handle->archive != nullptr;
}

void ZipFile::discard()
{
    _zip_handle->file.reset();
    _zip_handle->archive.reset();
    _current_zip_entry = ZipEntry {};
    _buf.clear();
}

Result<void> ZipFile::close()
{
    std::optional<Error> error;
    if (_zip_handle->archive != nullptr)
    {
        _zip_handle->file.reset();
        // release: zip_close commits and takes ownership, the deleter would discard
        zip_t *archive = _zip_handle->archive.release();
        if (zip_close(archive) != 0)
        {
            error = make_error(archive, "could not close zip file");
            zip_discard(archive);
        }
    }
    _current_zip_entry = ZipEntry {};
    _buf.clear();
    if (error)
        return std::unexpected(std::move(*error));
    return {};
}

std::string_view ZipFile::archive_comment() const
{
    if (_zip_handle->archive == nullptr)
        return "";
    int comment_length;
    const char *archive_comment = zip_get_archive_comment(_zip_handle->archive.get(), &comment_length, 0);
    return archive_comment != nullptr ? std::string_view(archive_comment, comment_length) : "";
}

Result<void> ZipFile::comment_archive(std::string_view comment)
{
    if (_zip_handle->archive == nullptr)
        return std::unexpected(not_open_error());
    if (zip_set_archive_comment(_zip_handle->archive.get(), comment.data(), comment.size()) != 0)
        return std::unexpected(make_error(_zip_handle->archive.get(), "could not write zip archive commentary"));
    return {};
}

Result<void> ZipFile::set_archive_readonly(bool active)
{
    if (_zip_handle->archive == nullptr)
        return std::unexpected(not_open_error());
    if (zip_set_archive_flag(_zip_handle->archive.get(), ZIP_AFL_RDONLY, (int)active) != 0)
        return std::unexpected(
            make_error(_zip_handle->archive.get(), "could not set archive {}", active ? "read-only" : "non read-only"));
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

Result<void> ZipFile::encrypt_all(const std::string & password)
{
    if (_zip_handle->archive == nullptr)
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
    if (_zip_handle->archive == nullptr)
        return -1;
    return zip_get_num_entries(_zip_handle->archive.get(), ZIP_FL_UNCHANGED);
}

ssize_t ZipFile::count_entries() const
{
    if (_zip_handle->archive == nullptr)
        return -1;
    return zip_get_num_entries(_zip_handle->archive.get(), 0);
}

Result<ZipFile::ZipEntry> ZipFile::load_entry(size_t idx)
{
    _current_zip_entry = ZipEntry {};
    _zip_handle->file.reset();

    if (_zip_handle->archive == nullptr)
        return std::unexpected(not_open_error());

    zip_stat_t zip_entry;
    zip_stat_init(&zip_entry);
    if (zip_stat_index(_zip_handle->archive.get(), idx, 0, &zip_entry) != 0)
        return std::unexpected(make_error(_zip_handle->archive.get(), "could not read entry '{}'", idx));
    save_entry(&zip_entry, _current_zip_entry);
    return _current_zip_entry;
}

Result<ZipFile::ZipEntry> ZipFile::load_entry(const std::string & name)
{
    _current_zip_entry = ZipEntry {};
    _zip_handle->file.reset();

    if (_zip_handle->archive == nullptr)
        return std::unexpected(not_open_error());

    zip_stat_t zip_entry;
    zip_stat_init(&zip_entry);
    if (zip_stat(_zip_handle->archive.get(), name.c_str(), 0, &zip_entry) != 0)
        return std::unexpected(make_error(_zip_handle->archive.get(), "could not read entry '{}'", name));
    save_entry(&zip_entry, _current_zip_entry);
    return _current_zip_entry;
}

bool ZipFile::is_entry_loaded() const
{
    return _zip_handle->archive != nullptr && _current_zip_entry.index >= 0;
}

Result<bool> ZipFile::read_next_entry()
{
    if (_zip_handle->archive == nullptr)
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
    const char *comment = zip_file_get_comment(_zip_handle->archive.get(),
                                               _current_zip_entry.index,
                                               &comment_length,
                                               0);
    return comment == nullptr ? "" : std::string_view(comment, comment_length);
}

bool ZipFile::is_entry_directory() const
{
    if (_zip_handle->archive == nullptr || _current_zip_entry.name == nullptr)
        return false;
    size_t len = strlen(_current_zip_entry.name);
    if (len == 0)
        return false;
    return _current_zip_entry.name[len - 1] == '/';
}

Result<bool> ZipFile::read_next()
{
    auto size_read = this->read_entry();
    SIHD_UNEXPECTED_RETURN(size_read);
    return *size_read > 0;
}

bool ZipFile::get_read_data(ArrCharView & view) const
{
    if (this->is_entry_loaded() == false)
        return false;
    view = {_buf.data(), _buf.size()};
    return true;
}

Result<ssize_t> ZipFile::read_entry(const std::string & password)
{
    _buf.clear();
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());

    if (_zip_handle->file.get() == nullptr)
    {
        if (password.empty() == false)
            _zip_handle->file.reset(
                zip_fopen_index_encrypted(_zip_handle->archive.get(), _current_zip_entry.index, 0, password.c_str()));
        else
            _zip_handle->file.reset(zip_fopen_index(_zip_handle->archive.get(), _current_zip_entry.index, 0));
        if (_zip_handle->file.get() == nullptr)
            return std::unexpected(
                make_error(_zip_handle->archive.get(), "could not open entry '{}'", _current_zip_entry.name));
    }

    if (_buf.capacity() == 0)
    {
        auto resized = this->set_buffer_size(4096);
        SIHD_UNEXPECTED_RETURN(resized);
    }

    _buf.resize(_buf.capacity());
    // -1 to account for the \0
    ssize_t ret = zip_fread(_zip_handle->file.get(), _buf.data(), _buf.capacity() - 1);
    if (ret < 0)
    {
        auto error = make_error(_zip_handle->file.get(), "could not read entry '{}'", _current_zip_entry.name);
        _buf.clear();
        _zip_handle->file.reset();
        return std::unexpected(std::move(error));
    }
    _buf.resize((size_t)ret);
    if (ret > 0)
        _buf[ret] = 0;
    if (ret == 0)
        _zip_handle->file.reset();
    return ret;
}

Result<void> ZipFile::unchange_entry()
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_unchange(_zip_handle->archive.get(), _current_zip_entry.index) < 0)
        return std::unexpected(make_error(_zip_handle->archive.get(),
                                          "could not remove changes to entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    return {};
}

Result<void> ZipFile::remove_entry()
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_delete(_zip_handle->archive.get(), _current_zip_entry.index) < 0)
        return std::unexpected(make_error(_zip_handle->archive.get(),
                                          "could not remove entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    return {};
}

Result<void> ZipFile::rename_entry(const std::string & new_name)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_file_rename(_zip_handle->archive.get(), _current_zip_entry.index, new_name.c_str(), 0) < 0)
        return std::unexpected(make_error(_zip_handle->archive.get(),
                                          "could not rename entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    auto reloaded = this->load_entry(_current_zip_entry.index);
    SIHD_UNEXPECTED_RETURN_CTX(reloaded,
                               "renaming entry '{}' to '{}'",
                               ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                               new_name);
    return {};
}

Result<void> ZipFile::modify_entry_time(Timestamp new_timestamp)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_file_set_mtime(_zip_handle->archive.get(), _current_zip_entry.index, new_timestamp.seconds(), 0) < 0)
        return std::unexpected(make_error(_zip_handle->archive.get(),
                                          "could not modify entry time '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    auto reloaded_2 = this->load_entry(_current_zip_entry.index);
    SIHD_UNEXPECTED_RETURN(reloaded_2);
    return {};
}

Result<void> ZipFile::comment_entry(std::string_view comment)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_file_set_comment(_zip_handle->archive.get(), _current_zip_entry.index, comment.data(), comment.size(), 0)
        < 0)
        return std::unexpected(make_error(_zip_handle->archive.get(),
                                          "could not comment entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    return {};
}

Result<void> ZipFile::encrypt_entry(const std::string & password)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());
    if (zip_file_set_encryption(_zip_handle->archive.get(),
                                _current_zip_entry.index,
                                _encryption_method,
                                password.c_str())
        < 0)
        return std::unexpected(make_error(_zip_handle->archive.get(),
                                          "failed to set encryption on index '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    return {};
}

Result<void> ZipFile::replace_entry(ArrCharView view)
{
    if (this->is_entry_loaded() == false)
        return std::unexpected(no_entry_error());

    _zip_handle->file.reset();

    zip_source_t *source = zip_source_buffer(_zip_handle->archive.get(), view.data(), view.byte_size(), 0);
    if (source == nullptr)
        return std::unexpected(make_error(_zip_handle->archive.get(),
                                          "could not create source buffer for entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));

    if (zip_file_replace(_zip_handle->archive.get(), _current_zip_entry.index, source, 0) != 0)
    {
        zip_source_free(source);
        return std::unexpected(make_error(_zip_handle->archive.get(),
                                          "could not replace entry '{}'",
                                          ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry)));
    }
    return {};
}

Result<void> ZipFile::add_dir(const std::string & name)
{
    if (_zip_handle->archive == nullptr)
        return std::unexpected(not_open_error());
    if (zip_dir_add(_zip_handle->archive.get(), name.c_str(), 0) < 0)
        return std::unexpected(make_error(_zip_handle->archive.get(), "could not add directory '{}'", name));
    return {};
}

Result<void> ZipFile::add_file(const std::string & name, ArrCharView view)
{
    if (_zip_handle->archive == nullptr)
        return std::unexpected(not_open_error());
    zip_source_t *source = zip_source_buffer(_zip_handle->archive.get(), view.data(), view.byte_size(), 0);
    if (source == nullptr)
        return std::unexpected(
            make_error(_zip_handle->archive.get(), "could not create source buffer for entry '{}'", name));
    return add_source(_zip_handle->archive.get(), name, source);
}

Result<void> ZipFile::add_from_fs(const std::string & name, const std::string & path)
{
    if (_zip_handle->archive == nullptr)
        return std::unexpected(not_open_error());

    if (fs::is_dir(path))
        return this->add_dir_from_fs(name, path);
    if (fs::is_file(path))
        return this->add_file_from_fs(name, path);

    return std::unexpected(Error(not_found, "no such file or directory '{}'", path));
}

Result<void> ZipFile::add_dir_from_fs(const std::string & name, const std::string & path)
{
    if (_zip_handle->archive == nullptr)
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
        std::string entry_name = name + "/" + child_name;
        if (auto added = this->add_from_fs(entry_name, fs::combine(path, child_name)); !added)
            return added;
    }
    return {};
}

Result<void> ZipFile::add_file_from_fs(const std::string & name, const std::string & path)
{
    if (_zip_handle->archive == nullptr)
        return std::unexpected(not_open_error());
    zip_source_t *source = zip_source_file(_zip_handle->archive.get(), path.c_str(), 0, 0);
    if (source == nullptr)
        return std::unexpected(
            make_error(_zip_handle->archive.get(), "could not create source file for entry '{}'", name));
    return add_source(_zip_handle->archive.get(), name, source);
}

Result<void> ZipFile::dump_entry_to_fs(const std::string & path, const std::string & password)
{
    if (_zip_handle->archive == nullptr || !this->is_entry_loaded())
        return std::unexpected(no_entry_error());

    if (this->is_entry_directory())
    {
        auto res = fs::make_directory(path, 0750);
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
            _zip_handle->file.reset();
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
