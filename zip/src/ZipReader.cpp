#include <sihd/zip/ZipReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/File.hpp>

namespace sihd::zip
{

SIHD_UTIL_REGISTER_FACTORY(ZipReader)

SIHD_LOGGER;

using namespace sihd::util;

ZipReader::ZipReader(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent), _zip_ptr(nullptr), _buf_ptr(nullptr), _buf_total_size(4096)
{
    _only_load_entries = true;
    this->_init();
    this->add_conf("read_entry_names", &ZipReader::set_read_entry_names);
    this->add_conf("buffer_size", &ZipReader::set_buffer_size);
    this->add_conf("password", &ZipReader::set_password);
}

ZipReader::~ZipReader()
{
    this->_delete_buffer();
    this->discard();
}

void    ZipReader::_close_file()
{
    if (_current_zip_file_ptr != nullptr)
    {
        zip_fclose(_current_zip_file_ptr);
        _current_zip_file_ptr = nullptr;
        _zip_reading_file = false;
        _read_buf_size = 0;
    }
}

void    ZipReader::_init()
{
    _total_entries = 0;
    _current_idx = 0;
    _entry_error = false;
    _zip_reading_file = false;
    _current_zip_file_ptr = nullptr;
}

void    ZipReader::_delete_buffer()
{
    if (_buf_ptr != nullptr)
    {
        delete[] _buf_ptr;
        _buf_ptr = nullptr;
    }
}

bool    ZipReader::_allocate_buffer_if_null()
{
    if (_buf_ptr == nullptr)
    {
        _buf_ptr = new char[_buf_total_size];
        if (_buf_ptr == nullptr)
        {
            SIHD_LOG(error, "ZipReader: could not allocate read buffer of size: {}", _buf_total_size);
        }
        else
        {
            _buf_ptr[0] = 0;
            _buf_ptr[_buf_total_size - 1] = 0;
        }
    }
    return _buf_ptr != nullptr;
}

bool    ZipReader::set_read_entry_names(bool active)
{
    _only_load_entries = active;
    return true;
}

bool    ZipReader::set_password(std::string_view password)
{
    if (_zip_ptr == nullptr)
        return false;
    if (zip_set_default_password(_zip_ptr, password.data()) < 0)
    {
        SIHD_LOG(error, "ZipReader: could not set password: {}", ZipUtils::get_error(_zip_ptr));
        return false;
    }
    return true;
}

bool    ZipReader::set_buffer_size(size_t size)
{
    if (size == 0)
    {
        SIHD_LOG(error, "ZipReader: cannot set buffer to 0");
        return false;
    }
    this->_delete_buffer();
    _buf_total_size = size;
    return this->_allocate_buffer_if_null();
}

bool    ZipReader::open(std::string_view path, bool do_strict_checks)
{
    int flags = ZIP_RDONLY;
    int error;

    if (do_strict_checks)
        flags |= ZIP_CHECKCONS;
    this->close();
    _zip_ptr = zip_open(path.data(), flags, &error);
    if (_zip_ptr == nullptr)
    {
        SIHD_LOG(error, "ZipReader: could not open zip: {}", ZipUtils::get_error(error));
        return false;
    }
    this->_init();
    _total_entries = zip_get_num_entries(_zip_ptr, 0);
    return this->_allocate_buffer_if_null();
}

void    ZipReader::discard()
{
    if (_zip_ptr != nullptr)
    {
        this->_close_file();
        zip_discard(_zip_ptr);
        _zip_ptr = nullptr;
    }
}

bool    ZipReader::close()
{
    bool ret = true;
    if (_zip_ptr != nullptr)
    {
        this->_close_file();
        ret = zip_close(_zip_ptr) == 0;
        if (!ret)
            SIHD_LOG(error, "ZipReader: could not close zip file: {}", ZipUtils::get_error(_zip_ptr));
        _zip_ptr = nullptr;
    }
    return ret;
}

bool    ZipReader::remove(size_t index)
{
    if (_zip_ptr == nullptr)
        return false;
    if (zip_delete(_zip_ptr, index) < 0)
    {
        SIHD_LOG(error, "ZipReader: could not remove index '{}': {}", index, ZipUtils::get_error(_zip_ptr));
        return false;
    }
    return true;
}

bool    ZipReader::rename(size_t index, std::string_view name)
{
    if (_zip_ptr == nullptr)
        return false;
    if (zip_file_rename(_zip_ptr, index, name.data(), ZIP_FL_ENC_UTF_8) < 0)
    {
        SIHD_LOG(error, "ZipReader: could not rename index '{}': {}", index, ZipUtils::get_error(_zip_ptr));
        return false;
    }
    return true;
}

bool    ZipReader::read_next()
{
    if (_current_idx >= _total_entries)
        return false;
    bool ret = true;
    if (_only_load_entries == true && _zip_reading_file == true)
        this->_close_file();
    if (_zip_reading_file == false)
    {
        ret = this->load_entry(_current_idx);
        if (ret &&_only_load_entries)
        {
            if (_current_zip_entry.name != nullptr)
            {
                strncpy(_buf_ptr, _current_zip_entry.name, _buf_total_size);
                _read_buf_size = strlen(_buf_ptr);
            }
            else
            {
                _buf_ptr[0] = 0;
                _read_buf_size = 0;
            }
        }
    }
    if (ret && _only_load_entries == false)
        ret = this->read_entry() >= 0;
    ++_current_idx;
    return ret;
}

bool    ZipReader::get_read_data(char **data, size_t *size) const
{
    if (_zip_ptr == nullptr)
        return false;
    *data = _buf_ptr;
    *size = _read_buf_size < 0 ? 0 : (size_t)_read_buf_size;
    return true;
}

bool    ZipReader::read_timestamp(time_t *nano_timestamp) const
{
    if (_zip_ptr == nullptr)
        return false;
    *nano_timestamp = _current_zip_entry.mtime;
    return true;
}

bool    ZipReader::load_entry(size_t idx)
{
    if (_zip_ptr == nullptr)
        return false;
    zip_stat_init(&_current_zip_entry);
    _entry_error = zip_stat_index(_zip_ptr, _current_idx, 0, &_current_zip_entry) != 0;
    if (_entry_error)
    {
        SIHD_LOG(error, "ZipReader: could not read entry '{}': {}", idx, ZipUtils::get_error(_zip_ptr));
    }
    return !_entry_error;
}

bool    ZipReader::load_entry(std::string_view name)
{
    if (_zip_ptr == nullptr)
        return false;
    if (_zip_reading_file)
        this->_close_file();
    zip_stat_init(&_current_zip_entry);
    _entry_error = zip_stat(_zip_ptr, name.data(), 0, &_current_zip_entry) != 0;
    if (_entry_error)
    {
        SIHD_LOG(error, "ZipReader: could not read entry '{}': {}", name, ZipUtils::get_error(_zip_ptr));
    }
    return !_entry_error;
}

const struct zip_stat *ZipReader::get_entry() const
{
    return _entry_error ? nullptr : &_current_zip_entry;
}

bool    ZipReader::is_entry_directory() const
{
    if (_zip_ptr == nullptr || _current_zip_entry.name == nullptr)
        return false;
    size_t len = strlen(_current_zip_entry.name);
    if (_current_zip_entry.name[len - 1] == '/')
        return true;
    return false;
}

ssize_t     ZipReader::read_entry(std::string_view password)
{
    if (_zip_ptr == nullptr)
        return false;
    if (_current_zip_file_ptr == nullptr)
    {
        if (password.empty() == false)
            _current_zip_file_ptr = zip_fopen_index_encrypted(_zip_ptr, _current_zip_entry.index, 0, password.data());
        else
            _current_zip_file_ptr = zip_fopen_index(_zip_ptr, _current_zip_entry.index, 0);
        if (_current_zip_file_ptr == nullptr)
        {
            SIHD_LOG(error, "ZipReader: could not open entry: {}", _current_zip_entry.name);
            return -1;
        }
        _zip_reading_file = true;
    }
    ssize_t ret = zip_fread(_current_zip_file_ptr, _buf_ptr, _buf_total_size);
    _read_buf_size = ret;
    if (ret < 0)
    {
        SIHD_LOG(error, "ZipReader: could not read entry: {}", _current_zip_entry.name);
    }
    else
        _buf_ptr[ret] = 0;
    if (ret <= 0)
        this->_close_file();
    return ret;
}

bool    ZipReader::write_entry(std::string_view path, std::string_view password)
{
    if (_zip_ptr == nullptr)
        return false;
    bool ret = false;
    if (this->is_entry_directory())
    {
        ret = fs::make_directory(path.data(), 0640);
        if (!ret)
            SIHD_LOG(error, "ZipReader: could not write directory entry '{}' to: {}", _current_zip_entry.name, path);
        return ret;
    }
    File file(path, "w");
    if (file.is_open())
    {
        ssize_t read;
        while (ret && (read = this->read_entry(password)) > 0)
        {
            ret = file.write(_buf_ptr, read) > 0;
            if (!ret)
            {
                SIHD_LOG(error, "ZipReader: could not write file entry '{}' to: {}", _current_zip_entry.name, path);
                this->_close_file();
            }
        }
        file.close();
    }
    return ret;
}

}
