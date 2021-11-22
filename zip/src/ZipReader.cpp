#include <sihd/zip/ZipReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/Files.hpp>

namespace sihd::zip
{

SIHD_UTIL_REGISTER_FACTORY(ZipReader)

LOGGER;

using namespace sihd::util;

ZipReader::ZipReader(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent), _zip_ptr(nullptr), _buf_size(4096), _buf_ptr(nullptr)
{
    this->add_conf("buf_size", &ZipReader::set_buff_size);
    this->add_conf("password", &ZipReader::set_password);
}

ZipReader::~ZipReader()
{
    this->_delete_buffer();
    this->discard();
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
        _buf_ptr = new char[_buf_size];
        if (_buf_ptr == nullptr)
            LOG(error, "ZipReader: could not allocate read buffer of size: " << _buf_size);
    }
    return _buf_ptr != nullptr;
}

bool    ZipReader::set_password(const char *password)
{
    if (_zip_ptr == nullptr)
        return false;
    if (zip_set_default_password(_zip_ptr, password) < 0)
    {
        LOG(error, "ZipReader: could not set password: " << ZipUtils::get_error(_zip_ptr));
        return false;
    }
    return true;
}

bool    ZipReader::set_buff_size(size_t size)
{
    this->_delete_buffer();
    _buf_size = size;
    return this->_allocate_buffer_if_null();
}

bool    ZipReader::open(const std::string & path, bool do_strict_checks)
{
    int flags = ZIP_RDONLY;
    int error;

    if (do_strict_checks)
        flags |= ZIP_CHECKCONS;
    this->close();
    _zip_ptr = zip_open(path.c_str(), flags, &error);
    if (_zip_ptr == nullptr)
        LOG(error, "ZipReader: could not open zip: " << ZipUtils::get_error(error));
    return _zip_ptr != nullptr && this->_allocate_buffer_if_null();
}

void    ZipReader::discard()
{
    if (_zip_ptr != nullptr)
    {
        zip_discard(_zip_ptr);
        _zip_ptr = nullptr;
    }
}

bool    ZipReader::close()
{
    bool ret = true;
    if (_zip_ptr != nullptr)
    {
        ret = zip_close(_zip_ptr) == 0;
        if (!ret)
            LOG(error, "ZipReader: could not close zip file: " << ZipUtils::get_error(_zip_ptr));
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
        LOG(error, "ZipReader: could not remove index '" << index << "': " << ZipUtils::get_error(_zip_ptr));
        return false;
    }
    return true;
}

bool    ZipReader::rename(size_t index, const char *name)
{
    if (_zip_ptr == nullptr)
        return false;
    if (zip_file_rename(_zip_ptr, index, name, ZIP_FL_ENC_UTF_8) < 0)
    {
        LOG(error, "ZipReader: could not rename index '" << index << "': " << ZipUtils::get_error(_zip_ptr));
        return false;
    }
    return true;
}

bool    ZipReader::is_entry(const std::string & name)
{
    zip_stat_t entry;
    return this->get_entry(name, &entry);    
}

bool    ZipReader::get_entry(const std::string & name, struct zip_stat *entry)
{
    if (_zip_ptr == nullptr)
        return false;
    zip_stat_init(entry);
    if (zip_stat(_zip_ptr, name.c_str(), 0, entry) == 0)
        return true;
    return false;
}

bool    ZipReader::get_entries(std::vector<struct zip_stat> & entries)
{
    if (_zip_ptr == nullptr)
        return false;
    struct zip_stat stat;
    size_t total_entries =  zip_get_num_entries(_zip_ptr, 0);
    size_t i = 0;
    while (i < total_entries)
    {
        zip_stat_init(&stat);
        if (zip_stat_index(_zip_ptr, i, 0, &stat) == 0)
        {
            entries.push_back(stat);
        }
        else
        {
            LOG(error, "ZipReader: could not read entry '" << i << "': " << ZipUtils::get_error(_zip_ptr));
            return false;
        }
        ++i;
    }
    return true;
}

size_t  ZipReader::count_entries()
{
    size_t ret = 0;
    if (_zip_ptr != nullptr)
        ret = zip_get_num_entries(_zip_ptr, 0);
    return ret;
}

bool    ZipReader::fs_write_entry(const struct zip_stat *entry, const std::string & output_path, const char *password)
{
    // directory
    size_t len = strlen(entry->name);
    if (entry->name[len - 1] == '/')
    {
        bool ret = Files::make_directory(output_path, 0640);
        if (!ret)
            LOG(error, "ZipReader: could not write directory entry '" << entry->name << "' to: " << output_path);
        return ret;
    }
    // file
    std::ofstream os(output_path, std::ofstream::out | std::ofstream::trunc);
    if (os.is_open() == false)
    {
        LOG(error, "ZipReader: could not open file to write entry '" << entry->name << "': " << output_path);
        return false;
    }
    struct zip_file *zf;
    if (password != nullptr)
        zf = zip_fopen_index_encrypted(_zip_ptr, entry->index, 0, password);
    else
        zf = zip_fopen_index(_zip_ptr, entry->index, 0);
    if (zf == nullptr)
    {
        LOG(error, "ZipReader: could not open entry: " << entry->name);
        return false;
    }
    bool good = true;
    ssize_t ret;
    size_t total = 0;
    while (total < entry->size)
    {
        ret = zip_fread(zf, _buf_ptr, _buf_size);
        if (ret < 0)
        {
            good = false;
            LOG(error, "ZipReader: could not write entry '" << entry->name << "' to: " << output_path);
            break ;
        }
        os.write(_buf_ptr, ret);
        total += ret;
    }
    zip_fclose(zf);
    os.close();
    return good && os.good();
}

bool    ZipReader::fs_write_entry(const std::string & name, const std::string & output_path, const char *password)
{
    struct zip_stat entry;
    if (this->get_entry(name, &entry))
        return this->fs_write_entry(&entry, output_path, password);
    return false;
}

bool    ZipReader::fs_write_all(const std::string & output_path)
{
    std::vector<struct zip_stat> entries;
    if (this->get_entries(entries) == false)
        return false;
    bool ret = true;
    for (const struct zip_stat & entry: entries)
    {
        ret = this->fs_write_entry(&entry, Files::combine(output_path, entry.name));
        if (!ret)
            break ;
    }
    return ret;
}

ssize_t ZipReader::read_entry(const struct zip_stat *entry, void *buf, size_t size, const char *password)
{
    // directory
    size_t len = strlen(entry->name);
    if (entry->name[len - 1] == '/')
        return 0;
    // file
    struct zip_file *zf;
    if (password != nullptr)
        zf = zip_fopen_index_encrypted(_zip_ptr, entry->index, 0, password);
    else
        zf = zip_fopen_index(_zip_ptr, entry->index, 0);
    if (zf == nullptr)
    {
        LOG(error, "ZipReader: could not open entry: " << entry->name);
        return -1;
    }
    ssize_t ret;
    size_t total = 0;
    while (total < entry->size)
    {
        ret = zip_fread(zf, buf, size);
        if (ret < 0)
        {
            LOG(error, "ZipReader: could not read entry: " << entry->name);
            return -1;
        }
        total += ret;
    }
    zip_fclose(zf);
    return total;
}

ssize_t ZipReader::read_entry(const struct zip_stat *entry, const char *password)
{
    return this->read_entry(entry, _buf_ptr, _buf_size, password);
}

bool    ZipReader::read_entry_into(const struct zip_stat *entry, std::string & into, const char *password)
{
    ssize_t ret = this->read_entry(entry, password);
    if (ret > 0)
    {
        _buf_ptr[ret] = 0;
        into.append(_buf_ptr);
    }
    return ret >= 0;
}

ssize_t ZipReader::read_entry(const std::string & name, void *buf, size_t size, const char *password)
{
    zip_stat_t entry;
    zip_stat_init(&entry);
    if (this->get_entry(name, &entry))
        return this->read_entry(&entry, buf, size, password);
    return -1;
}

ssize_t ZipReader::read_entry(const std::string & name, const char *password)
{
    zip_stat_t entry;
    zip_stat_init(&entry);
    if (this->get_entry(name, &entry))
        return this->read_entry(&entry, password);
    return -1;
}

bool    ZipReader::read_entry_into(const std::string & name, std::string & into, const char *password)
{
    zip_stat_t entry;
    zip_stat_init(&entry);
    if (this->get_entry(name, &entry))
        return this->read_entry_into(&entry, into, password);
    return false;
}


}