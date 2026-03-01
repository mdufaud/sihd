#include <zip.h>

#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>

#include <sihd/zip/ZipFile.hpp>
#include <sihd/zip/zip.hpp>

#define ZIP_ENTRY_NAME_OR_INDEX(entry) (entry.name != nullptr ? entry.name : std::to_string(entry.index))

namespace sihd::zip
{

SIHD_NEW_LOGGER("sihd::zip");

namespace
{

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
        zip_entry.modification_time = zip_stat->mtime;
    if (zip_stat->valid & ZIP_STAT_CRC)
        zip_entry.crc = zip_stat->crc;
}

ssize_t add_source(zip_t *zip_ptr, std::string_view name, zip_source_t *source)
{
    if (zip_ptr == nullptr)
        return false;
    const ssize_t file_idx = zip_file_add(zip_ptr, name.data(), source, ZIP_FL_ENC_UTF_8);
    if (file_idx < 0)
    {
        zip_source_free(source);
        SIHD_LOG(error, "ZipFile: could not add file '{}' to zip: '{}'", name, get_error(zip_ptr));
    }
    return file_idx;
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

using namespace sihd::util;
using namespace sihd::sys;

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
    this->open(path, read_only, do_strict_checks);
}

ZipFile::~ZipFile()
{
    this->discard();
}

bool ZipFile::set_password(std::string_view password)
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;
    if (zip_set_default_password(_zip_handle->handle_ptr, password.data()) < 0)
    {
        SIHD_LOG(error, "ZipFile: could not set password: {}", get_error(_zip_handle->handle_ptr));
        return false;
    }
    return true;
}

bool ZipFile::set_buffer_size(size_t size)
{
    if (size == 0)
    {
        SIHD_LOG(error, "ZipFile: cannot set buffer to 0");
        return false;
    }
    _buf.reserve(size + 1);
    return true;
}

bool ZipFile::set_aes_encryption(int aes)
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
            SIHD_LOG(error, "ZipWriter: no such encryption for AES: {}", aes);
            return false;
    }
    return true;
}

bool ZipFile::open(std::string_view path, bool read_only, bool do_strict_checks)
{
    this->close();

    int flags = ZIP_CREATE;
    if (read_only)
        flags |= ZIP_RDONLY;
    if (do_strict_checks)
        flags |= ZIP_CHECKCONS;

    int error;
    _zip_handle->handle_ptr = zip_open(path.data(), flags, &error);
    if (_zip_handle->handle_ptr == nullptr)
    {
        SIHD_LOG(error, "ZipFile: could not open zip: {}", get_error(error));
    }

    _current_zip_entry = ZipEntry {};
    _zip_handle->file_handle_ptr = nullptr;
    _buf.clear();

    return _zip_handle->handle_ptr != nullptr;
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

bool ZipFile::close()
{
    bool ret = true;
    if (_zip_handle->handle_ptr != nullptr)
    {
        close_zip_file_and_null(&_zip_handle->file_handle_ptr);
        ret = zip_close(_zip_handle->handle_ptr) == 0;
        if (!ret)
            SIHD_LOG(error, "ZipFile: could not close zip file: {}", get_error(_zip_handle->handle_ptr));
        _zip_handle->handle_ptr = nullptr;
    }
    _current_zip_entry = ZipEntry {};
    _buf.clear();
    return ret;
}

std::string_view ZipFile::archive_comment() const
{
    if (_zip_handle->handle_ptr == nullptr)
        return "";
    int comment_length;
    const char *archive_comment = zip_get_archive_comment(_zip_handle->handle_ptr, &comment_length, 0);
    return archive_comment != nullptr ? std::string_view(archive_comment, comment_length) : "";
}

bool ZipFile::comment_archive(std::string_view comment)
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;
    const bool success
        = zip_set_archive_comment(_zip_handle->handle_ptr, comment.data(), comment.size()) == 0;
    if (!success)
    {
        SIHD_LOG(error,
                 "ZipFile: could not write zip archive commentary: {}",
                 get_error(_zip_handle->handle_ptr));
    }
    return success;
}

bool ZipFile::set_archive_readonly(bool active)
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;
    const bool success = zip_set_archive_flag(_zip_handle->handle_ptr, ZIP_AFL_RDONLY, (int)active) == 0;
    if (!success)
    {
        SIHD_LOG(error,
                 "ZipFile: could not set archive {}: {}",
                 active ? "read-only" : "non read-only",
                 get_error(_zip_handle->handle_ptr));
    }
    return success;
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

bool ZipFile::encrypt_all(std::string_view password)
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;

    const size_t total_entries = this->count_entries();
    size_t idx = 0;
    while (idx < total_entries)
    {
        const bool success = this->load_entry(idx) && this->encrypt_entry(password);
        if (!success)
            return false;
        ++idx;
    }
    return true;
}

ssize_t ZipFile::count_original_entries() const
{
    return zip_get_num_entries(_zip_handle->handle_ptr, ZIP_FL_UNCHANGED);
}

ssize_t ZipFile::count_entries() const
{
    return zip_get_num_entries(_zip_handle->handle_ptr, 0);
}

bool ZipFile::load_entry(size_t idx)
{
    _current_zip_entry = ZipEntry {};
    close_zip_file_and_null(&_zip_handle->file_handle_ptr);

    if (_zip_handle->handle_ptr == nullptr)
        return false;

    zip_stat_t zip_entry;
    zip_stat_init(&zip_entry);
    const bool success = zip_stat_index(_zip_handle->handle_ptr, idx, 0, &zip_entry) == 0;
    if (success)
    {
        save_entry(&zip_entry, _current_zip_entry);
    }
    else
    {
        SIHD_LOG(error, "ZipFile: could not read entry '{}': {}", idx, get_error(_zip_handle->handle_ptr));
    }
    return success;
}

bool ZipFile::load_entry(std::string_view name)
{
    _current_zip_entry = ZipEntry {};
    close_zip_file_and_null(&_zip_handle->file_handle_ptr);

    if (_zip_handle->handle_ptr == nullptr)
        return false;

    zip_stat_t zip_entry;
    zip_stat_init(&zip_entry);
    const bool success = zip_stat(_zip_handle->handle_ptr, name.data(), 0, &zip_entry) == 0;
    if (success)
    {
        save_entry(&zip_entry, _current_zip_entry);
    }
    else
    {
        SIHD_LOG(error, "ZipFile: could not read entry '{}': {}", name, get_error(_zip_handle->handle_ptr));
    }
    return success;
}

bool ZipFile::is_entry_loaded() const
{
    return _zip_handle->handle_ptr != nullptr && _current_zip_entry.index >= 0;
}

bool ZipFile::read_next_entry()
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;
    const ssize_t total_entries = this->count_original_entries();
    if (_current_zip_entry.index + 1 >= total_entries)
        return false;
    return this->load_entry(_current_zip_entry.index + 1);
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
    const char *comment
        = zip_file_get_comment(_zip_handle->handle_ptr, _current_zip_entry.index, &comment_length, 0);
    return comment == nullptr ? "" : std::string_view(comment, comment_length);
}

bool ZipFile::is_entry_directory() const
{
    if (_zip_handle->handle_ptr == nullptr || _current_zip_entry.name == nullptr)
        return false;
    size_t len = strlen(_current_zip_entry.name);
    if (_current_zip_entry.name[len - 1] == '/')
        return true;
    return false;
}

bool ZipFile::read_next()
{
    const ssize_t size_read = this->read_entry();
    return size_read > 0;
}

bool ZipFile::get_read_data(sihd::util::ArrCharView & view) const
{
    if (this->is_entry_loaded() == false)
        return false;
    view = {_buf.data(), _buf.size()};
    return true;
}

ssize_t ZipFile::read_entry(std::string_view password)
{
    _buf.clear();
    if (this->is_entry_loaded() == false)
        return false;

    if (_zip_handle->file_handle_ptr == nullptr)
    {
        if (password.empty() == false)
            _zip_handle->file_handle_ptr = zip_fopen_index_encrypted(_zip_handle->handle_ptr,
                                                                     _current_zip_entry.index,
                                                                     0,
                                                                     password.data());
        else
            _zip_handle->file_handle_ptr
                = zip_fopen_index(_zip_handle->handle_ptr, _current_zip_entry.index, 0);
        if (_zip_handle->file_handle_ptr == nullptr)
        {
            SIHD_LOG(error, "ZipFile: could not open entry: {}", _current_zip_entry.name);
            return -1;
        }
    }

    if (_buf.capacity() == 0)
        this->set_buffer_size(4096);

    _buf.resize(_buf.capacity());
    // -1 to account for the \0
    ssize_t ret = zip_fread(_zip_handle->file_handle_ptr, _buf.data(), _buf.capacity() - 1);
    if (ret < 0)
    {
        SIHD_LOG(error, "ZipFile: could not read entry: {}", _current_zip_entry.name);
        _buf.clear();
    }
    else
    {
        _buf.resize((size_t)ret);
        _buf[ret] = 0;
    }
    if (ret <= 0)
        close_zip_file_and_null(&_zip_handle->file_handle_ptr);
    return ret;
}

bool ZipFile::unchange_entry()
{
    if (this->is_entry_loaded() == false)
        return false;
    if (zip_unchange(_zip_handle->handle_ptr, _current_zip_entry.index) < 0)
    {
        SIHD_LOG(error,
                 "ZipFile: could not remove changes to entry '{}': {}",
                 ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                 get_error(_zip_handle->handle_ptr));
        return false;
    }
    return true;
}

bool ZipFile::remove_entry()
{
    if (this->is_entry_loaded() == false)
        return false;
    if (zip_delete(_zip_handle->handle_ptr, _current_zip_entry.index) < 0)
    {
        SIHD_LOG(error,
                 "ZipFile: could not remove entry '{}': {}",
                 ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                 get_error(_zip_handle->handle_ptr));
        return false;
    }
    return true;
}

bool ZipFile::rename_entry(std::string_view new_name)
{
    if (this->is_entry_loaded() == false)
        return false;
    if (zip_file_rename(_zip_handle->handle_ptr, _current_zip_entry.index, new_name.data(), 0) < 0)
    {
        SIHD_LOG(error,
                 "ZipFile: could not rename entry '{}': {}",
                 ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                 get_error(_zip_handle->handle_ptr));
        return false;
    }
    // reload new data
    return this->load_entry(_current_zip_entry.index);
}

bool ZipFile::modify_entry_time(sihd::util::Timestamp new_timestamp)
{
    if (this->is_entry_loaded() == false)
        return false;
    if (zip_file_set_mtime(_zip_handle->handle_ptr, _current_zip_entry.index, new_timestamp.seconds(), 0) < 0)
    {
        SIHD_LOG(error,
                 "ZipFile: could not rename entry '{}': {}",
                 ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                 get_error(_zip_handle->handle_ptr));
        return false;
    }
    // reload new data
    return this->load_entry(_current_zip_entry.index);
}

bool ZipFile::comment_entry(std::string_view comment)
{
    if (this->is_entry_loaded() == false)
        return false;
    if (zip_file_set_comment(_zip_handle->handle_ptr,
                             _current_zip_entry.index,
                             comment.data(),
                             comment.size(),
                             0)
        < 0)
    {
        SIHD_LOG(error,
                 "ZipFile: could not rename entry '{}': {}",
                 ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                 get_error(_zip_handle->handle_ptr));
        return false;
    }
    return true;
}

bool ZipFile::encrypt_entry(std::string_view password)
{
    if (this->is_entry_loaded() == false)
        return false;

    const bool ret = zip_file_set_encryption(_zip_handle->handle_ptr,
                                             _current_zip_entry.index,
                                             _encryption_method,
                                             password.data())
                     == 0;
    if (!ret)
        SIHD_LOG(error,
                 "ZipFile: failed to set encryption on index '{}': {}",
                 ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                 get_error(_zip_handle->handle_ptr));
    return ret;
}

bool ZipFile::replace_entry(sihd::util::ArrCharView view)
{
    if (this->is_entry_loaded() == false)
        return false;

    close_zip_file_and_null(&_zip_handle->file_handle_ptr);

    zip_source_t *source = zip_source_buffer(_zip_handle->handle_ptr, view.data(), view.byte_size(), 0);
    if (source == nullptr)
    {
        SIHD_LOG(error,
                 "ZipFile: could not create source buffer for entry '{}' to zip: {}",
                 ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                 get_error(_zip_handle->handle_ptr));
        return false;
    }

    const bool success = zip_file_replace(_zip_handle->handle_ptr, _current_zip_entry.index, source, 0) == 0;
    if (!success)
    {
        zip_source_free(source);
        SIHD_LOG(error,
                 "ZipFile: could not replace entry '{}' to zip: {}",
                 ZIP_ENTRY_NAME_OR_INDEX(_current_zip_entry),
                 get_error(_zip_handle->handle_ptr));
    }
    return success;
}

bool ZipFile::add_dir(std::string_view name)
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;
    if (zip_dir_add(_zip_handle->handle_ptr, name.data(), 0) < 0)
        return false;
    return true;
}

bool ZipFile::add_file(std::string_view name, sihd::util::ArrCharView view)
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;
    zip_source_t *source = zip_source_buffer(_zip_handle->handle_ptr, view.data(), view.byte_size(), 0);
    if (source == nullptr)
    {
        SIHD_LOG(error,
                 "ZipFile: could not create source buffer for entry '{}' to zip: {}",
                 name,
                 get_error(_zip_handle->handle_ptr));
        return false;
    }
    const ssize_t file_idx = add_source(_zip_handle->handle_ptr, name, source);
    return file_idx >= 0;
}

bool ZipFile::add_from_fs(std::string_view name, std::string_view path)
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;

    if (fs::is_dir(path))
        return this->add_dir_from_fs(name, path);
    else if (fs::is_file(path))
        return this->add_file_from_fs(name, path);

    return false;
}

bool ZipFile::add_dir_from_fs(std::string_view name, std::string_view path)
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;
    if (this->add_dir(name) == false)
    {
        SIHD_LOG(error,
                 "ZipFile: could not add directory '{}' to zip: {}",
                 name,
                 get_error(_zip_handle->handle_ptr));
        return false;
    }
    std::vector<std::string> children = fs::children(path);
    for (std::string_view child : children)
    {
        if (this->add_from_fs(fs::combine(name, child), fs::combine(path, child)) == false)
            return false;
    }
    return true;
}

bool ZipFile::add_file_from_fs(std::string_view name, std::string_view path)
{
    if (_zip_handle->handle_ptr == nullptr)
        return false;
    zip_source_t *source = zip_source_file(_zip_handle->handle_ptr, path.data(), 0, 0);
    if (source == nullptr)
    {
        SIHD_LOG(error,
                 "ZipFile: could not create source file for entry '{}' to zip: {}",
                 name,
                 get_error(_zip_handle->handle_ptr));
        return false;
    }
    return add_source(_zip_handle->handle_ptr, name, source);
}

bool ZipFile::dump_entry_to_fs(std::string_view path, std::string_view password)
{
    if (_zip_handle->handle_ptr == nullptr || !this->is_entry_loaded())
        return false;

    if (this->is_entry_directory())
    {
        const bool success = fs::make_directory(path.data(), 0750);
        if (!success)
            SIHD_LOG(error,
                     "ZipFile: could not write directory entry '{}' - {} for: {}",
                     _current_zip_entry.name,
                     sys::os::last_error_str(),
                     path);
        return success;
    }

    File file(path, "w");

    if (!file.is_open())
    {
        SIHD_LOG(error,
                 "ZipFile: could not open file entry '{}' - {} for: {}",
                 _current_zip_entry.name,
                 sys::os::last_error_str(),
                 path);
        return false;
    }

    ssize_t read;
    ssize_t wrote;
    while ((read = this->read_entry(password)) > 0)
    {
        wrote = file.write(_buf.data(), read);
        if (wrote < 0)
        {
            SIHD_LOG(error,
                     "ZipFile: could not write file entry '{}' - {} for: {}",
                     _current_zip_entry.name,
                     sys::os::last_error_str(),
                     path);
            close_zip_file_and_null(&_zip_handle->file_handle_ptr);
            return false;
        }
        else if (wrote == 0)
            break;
    }
    return true;
}

} // namespace sihd::zip
