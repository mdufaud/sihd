#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/fs.hpp>

#include <sihd/zip/utils.hpp>
#include <sihd/zip/ZipWriter.hpp>

namespace sihd::zip
{

SIHD_UTIL_REGISTER_FACTORY(ZipWriter)

SIHD_LOGGER;

using namespace sihd::util;

ZipWriter::ZipWriter(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent), _zip_ptr(nullptr)
{
    this->no_encrypt();
    this->add_conf("aes", &ZipWriter::set_aes_encryption);
}

ZipWriter::~ZipWriter()
{
    this->close();
}

bool    ZipWriter::set_aes_encryption(int aes)
{
    switch (aes)
    {
        case 0:
            this->no_encrypt();
            break ;
        case 128:
            this->encrypt_in_aes_128();
            break ;
        case 192:
            this->encrypt_in_aes_192();
            break ;
        case 256:
            this->encrypt_in_aes_256();
            break ;
        default:
            SIHD_LOG(error, "ZipWriter: no such encryption for AES: {}", aes);
            return false;
    }
    return true;
}

void    ZipWriter::no_encrypt()
{
    _encryption_method = ZIP_EM_NONE;
}

void    ZipWriter::encrypt_in_aes_128()
{
    _encryption_method = ZIP_EM_AES_128;
}

void    ZipWriter::encrypt_in_aes_192()
{
    _encryption_method = ZIP_EM_AES_192;
}

void    ZipWriter::encrypt_in_aes_256()
{
    _encryption_method = ZIP_EM_AES_256;
}

bool    ZipWriter::open(std::string_view path, bool fails_if_exists, bool truncate)
{
    int flags = ZIP_CREATE;
    int error;

    if (fails_if_exists)
        flags |= ZIP_EXCL;
    if (truncate)
        flags |= ZIP_TRUNCATE;
    this->close();
    _zip_ptr = zip_open(path.data(), flags, &error);
    if (_zip_ptr == nullptr)
        SIHD_LOG(error, "ZipWriter: could not open zip: {}", utils::get_error(error));
    return _zip_ptr != nullptr;
}

bool    ZipWriter::close()
{
    bool ret = true;
    if (_zip_ptr != nullptr)
    {
        ret = zip_close(_zip_ptr) == 0;
        if (!ret)
            SIHD_LOG(error, "ZipWriter: could not close zip file: {}", utils::get_error(_zip_ptr));
        _zip_ptr = nullptr;
    }
    return ret;
}

bool    ZipWriter::add_dir(std::string_view name)
{
    if (_zip_ptr == nullptr)
        return false;
    if (zip_dir_add(_zip_ptr, name.data(), ZIP_FL_ENC_UTF_8) < 0)
        return false;
    return true;
}

bool    ZipWriter::fs_add(std::string_view path, std::string_view name)
{
    if (_zip_ptr == nullptr)
        return false;
    if (fs::is_dir(path))
        return this->fs_add_dir(path, name);
    else if (fs::is_file(path))
        return this->fs_add_file(path, name);
    return false;
}

bool    ZipWriter::fs_add_dir(std::string_view path, std::string_view name)
{
    if (_zip_ptr == nullptr)
        return false;
    if (this->add_dir(name) == false)
    {
        SIHD_LOG(error, "ZipWriter: could not add directory to zip '{}' for: {}", utils::get_error(_zip_ptr), path);
        return false;
    }
    std::vector<std::string> children = fs::children(path);
    bool ret = true;
    for (std::string_view child: children)
    {
        ret = this->fs_add(fs::combine(path, child), fs::combine(name, child));
        if (!ret)
            break ;
    }
    return ret;
}

bool    ZipWriter::add_file(std::string_view name, sihd::util::ArrCharView view)
{
    if (_zip_ptr == nullptr)
        return false;
    zip_source_t *source = zip_source_buffer(_zip_ptr, view.data(), view.byte_size(), 0);
    if (source == nullptr)
    {
        SIHD_LOG(error, "ZipWriter: could not add file to zip '{}' for: {}", utils::get_error(_zip_ptr), name);
        return false;
    }
    return this->_add_source(name, source);
}

bool    ZipWriter::fs_add_file(std::string_view path, std::string_view name)
{
    if (_zip_ptr == nullptr)
        return false;
    zip_source_t *source = zip_source_file(_zip_ptr, path.data(), 0, 0);
    if (source == nullptr)
    {
        SIHD_LOG(error, "ZipWriter: could not add file to zip '{}' for: {}", utils::get_error(_zip_ptr), path);
        return false;
    }
    return this->_add_source(name, source);
}

bool    ZipWriter::_add_source(std::string_view name, zip_source_t *source)
{
    if (_zip_ptr == nullptr)
        return false;
    int ret = zip_file_add(_zip_ptr, name.data(), source, ZIP_FL_ENC_UTF_8);
    if (ret < 0)
    {
        SIHD_LOG(error, "ZipWriter: could not add file to zip '{}' for: {}", utils::get_error(_zip_ptr), name);
        zip_source_free(source);
    }
    return ret >= 0;
}

bool    ZipWriter::encrypt(size_t index, std::string_view password)
{
    if (_zip_ptr == nullptr)
        return false;
    bool ret = zip_file_set_encryption(_zip_ptr, index, _encryption_method, password.data()) >= 0;
    if (!ret)
        SIHD_LOG(error, "ZipWriter: failed to set encryption on index '{}': {}", index, utils::get_error(_zip_ptr));
    return ret;
}

bool    ZipWriter::encrypt_all(std::string_view password)
{
    if (_zip_ptr == nullptr)
        return false;
    size_t total_entries = zip_get_num_entries(_zip_ptr, 0);
    size_t i = 0;
    while (i < total_entries)
    {
        if (this->encrypt(i, password) == false)
            return false;
        ++i;
    }
    return true;
}

}
