#include <fcntl.h>

#include <cstring>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include <sihd/ssh/Sftp.hpp>
#include <sihd/ssh/utils.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fmt.hpp>

#include "ssh_error.hpp"

#ifndef SIHD_SSH_SFTP_BUFSIZE
# define SIHD_SSH_SFTP_BUFSIZE 4096
#endif

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using namespace sihd::sys;

namespace sihd::ssh
{

namespace
{

struct SftpFileDeleter
{
        void operator()(sftp_file_struct *ptr)
        {
            if (ptr != nullptr)
                sftp_close(ptr);
        }
};

using SftpFilePtr = std::unique_ptr<sftp_file_struct, SftpFileDeleter>;

struct SftpDirDeleter
{
        void operator()(sftp_dir_struct *ptr)
        {
            if (ptr != nullptr)
                sftp_closedir(ptr);
        }
};

using SftpDirPtr = std::unique_ptr<sftp_dir_struct, SftpDirDeleter>;

struct SftpAttributeDeleter
{
        void operator()(sftp_attributes_struct *ptr)
        {
            if (ptr != nullptr)
                sftp_attributes_free(ptr);
        }
};

using SftpAttributePtr = std::unique_ptr<sftp_attributes_struct, SftpAttributeDeleter>;

} // namespace

SIHD_LOGGER;

struct Sftp::Impl
{
        ssh_session_struct *ssh_session_ptr {nullptr};
        sftp_session_struct *sftp_session_ptr {nullptr};
};

Sftp::Sftp(void *session): _impl_ptr(std::make_unique<Impl>())
{
    _impl_ptr->ssh_session_ptr = static_cast<ssh_session_struct *>(session);
    SIHD_UNEXPECTED_LOG(utils::init());
}

Sftp::~Sftp()
{
    this->close();
    SIHD_UNEXPECTED_LOG(utils::finalize());
}

std::expected<void, Error> Sftp::open()
{
    _impl_ptr->sftp_session_ptr = sftp_new(_impl_ptr->ssh_session_ptr);
    if (_impl_ptr->sftp_session_ptr == nullptr)
    {
        return std::unexpected(Error(_impl_ptr->ssh_session_ptr,
                                     "could not create sftp session: {}",
                                     ssh_error_str(_impl_ptr->ssh_session_ptr)));
    }
    if (sftp_init(_impl_ptr->sftp_session_ptr) != SSH_FX_OK)
    {
        Error err(_impl_ptr->sftp_session_ptr,
                  "could not initialize sftp session: {}",
                  sftp_error_str(_impl_ptr->sftp_session_ptr));
        this->close();
        return std::unexpected(std::move(err));
    }
    return {};
}

bool Sftp::is_open() const
{
    return _impl_ptr->sftp_session_ptr != nullptr;
}

void Sftp::close()
{
    if (_impl_ptr->sftp_session_ptr != nullptr)
    {
        sftp_free(_impl_ptr->sftp_session_ptr);
        _impl_ptr->sftp_session_ptr = nullptr;
    }
}

std::expected<void, Error> Sftp::send_file(std::string_view local_path, std::string_view remote_path, mode_t mode)
{
    sihd::sys::File local_file(local_path, "rb");
    if (local_file.is_open() == false)
        return std::unexpected(Error(io_error, "could not open local file '{}'", local_path));

    const int flags = O_WRONLY | O_CREAT | O_TRUNC;
    SftpFilePtr remote_file(sftp_open(_impl_ptr->sftp_session_ptr, remote_path.data(), flags, mode));
    if (remote_file.get() == nullptr)
    {
        return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                     "could not open remote file '{}': {}",
                                     remote_path,
                                     sftp_error_str(_impl_ptr->sftp_session_ptr)));
    }

    char buf[SIHD_SSH_SFTP_BUFSIZE + 1];
    while (true)
    {
        const auto read = local_file.read(buf, SIHD_SSH_SFTP_BUFSIZE);
        SIHD_UNEXPECTED_RETURN(read);
        if (*read == 0)
            break;
        const ssize_t nread = (ssize_t)*read;
        const int nwritten = sftp_write(remote_file.get(), buf, nread);
        if (nwritten != nread)
        {
            return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                         "could not write remote file '{}': wrote {} of {} bytes: {}",
                                         remote_path,
                                         nwritten,
                                         nread,
                                         sftp_error_str(_impl_ptr->sftp_session_ptr)));
        }
    }
    return {};
}

std::expected<void, Error> Sftp::get_file(std::string_view remote_path, std::string_view local_path)
{
    sihd::sys::File local_file(local_path, "wb");
    if (local_file.is_open() == false)
        return std::unexpected(Error(io_error, "could not open local file '{}'", local_path));

    const int flags = O_RDONLY;
    SftpFilePtr remote_file(sftp_open(_impl_ptr->sftp_session_ptr, remote_path.data(), flags, 0));
    if (remote_file.get() == nullptr)
    {
        return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                     "could not open remote file '{}': {}",
                                     remote_path,
                                     sftp_error_str(_impl_ptr->sftp_session_ptr)));
    }

    char buf[SIHD_SSH_SFTP_BUFSIZE + 1];
    while (true)
    {
        const ssize_t nread = sftp_read(remote_file.get(), buf, SIHD_SSH_SFTP_BUFSIZE);
        if (nread < 0)
        {
            return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                         "could not read remote file '{}': {}",
                                         remote_path,
                                         sftp_error_str(_impl_ptr->sftp_session_ptr)));
        }
        if (nread == 0)
            break;
        const auto nwritten = local_file.write(buf, nread);
        if (!nwritten || *nwritten != (size_t)nread)
        {
            return std::unexpected(Error(io_error,
                                         "could not write local file '{}' : wrote {} of {} bytes",
                                         local_path,
                                         nwritten.value_or(0),
                                         nread));
        }
    }
    return {};
}

std::expected<void, Error> Sftp::mkdir(std::string_view path, mode_t mode)
{
    if (sftp_mkdir(_impl_ptr->sftp_session_ptr, path.data(), mode) == SSH_FX_OK)
        return {};
    return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                 "could not mkdir '{}': {}",
                                 path,
                                 sftp_error_str(_impl_ptr->sftp_session_ptr)));
}

std::expected<void, Error> Sftp::symlink(std::string_view from, std::string_view to)
{
    if (sftp_symlink(_impl_ptr->sftp_session_ptr, from.data(), to.data()) == SSH_FX_OK)
        return {};
    return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                 "could not create symbolic link from '{}' to '{}': {}",
                                 from,
                                 to,
                                 sftp_error_str(_impl_ptr->sftp_session_ptr)));
}

std::expected<void, Error> Sftp::list_dir_filenames(std::string_view path, std::vector<std::string> & list)
{
    std::vector<SftpAttribute> attrs;
    if (auto res = this->list_dir(path, attrs); !res)
        return res;
    for (const SftpAttribute & attr : attrs)
    {
        if (attr.is_dir())
        {
            std::string name(attr.name());
            name += "/";
            list.push_back(std::move(name));
        }
        else
            list.push_back(std::string(attr.name()));
    }
    return {};
}

std::expected<void, Error> Sftp::list_dir(std::string_view path, std::vector<SftpAttribute> & list)
{
    SftpDirPtr dir(sftp_opendir(_impl_ptr->sftp_session_ptr, path.data()));
    if (dir.get() == nullptr)
    {
        return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                     "could not open directory '{}': {}",
                                     path,
                                     sftp_error_str(_impl_ptr->sftp_session_ptr)));
    }
    while (true)
    {
        SftpAttributePtr attr(sftp_readdir(_impl_ptr->sftp_session_ptr, dir.get()));
        if (attr.get() == nullptr)
            break;
        if (attr->name == nullptr || strcmp(attr->name, ".") == 0 || strcmp(attr->name, "..") == 0)
            continue;
        list.emplace_back(attr->name, attr->type, attr->size);
    }
    if (sftp_dir_eof(dir.get()) != 1)
    {
        return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                     "could not list directory '{}': {}",
                                     path,
                                     sftp_error_str(_impl_ptr->sftp_session_ptr)));
    }
    return {};
}

std::string Sftp::readlink(std::string_view path)
{
    char *ret = sftp_readlink(_impl_ptr->sftp_session_ptr, path.data());
    if (ret == nullptr)
        return "";
    return std::string(ret);
}

std::expected<void, Error> Sftp::rename(std::string_view from, std::string_view to)
{
    if (sftp_rename(_impl_ptr->sftp_session_ptr, from.data(), to.data()) == SSH_FX_OK)
        return {};
    return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                 "could not rename '{}' to '{}': {}",
                                 from,
                                 to,
                                 sftp_error_str(_impl_ptr->sftp_session_ptr)));
}

std::expected<void, Error> Sftp::rm(std::string_view path)
{
    if (sftp_unlink(_impl_ptr->sftp_session_ptr, path.data()) == SSH_FX_OK)
        return {};
    return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                 "could not remove '{}': {}",
                                 path,
                                 sftp_error_str(_impl_ptr->sftp_session_ptr)));
}

std::expected<void, Error> Sftp::rmdir(std::string_view path)
{
    if (sftp_rmdir(_impl_ptr->sftp_session_ptr, path.data()) == SSH_FX_OK)
        return {};
    return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                 "could not remove directory '{}': {}",
                                 path,
                                 sftp_error_str(_impl_ptr->sftp_session_ptr)));
}

std::expected<void, Error> Sftp::chmod(std::string_view path, mode_t mode)
{
    if (sftp_chmod(_impl_ptr->sftp_session_ptr, path.data(), mode) == SSH_FX_OK)
        return {};
    return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                 "could not change permission of '{}' ({}): {}",
                                 path,
                                 mode,
                                 sftp_error_str(_impl_ptr->sftp_session_ptr)));
}

std::expected<void, Error> Sftp::chown(std::string_view path, uid_t owner, gid_t group)
{
    if (sftp_chown(_impl_ptr->sftp_session_ptr, path.data(), owner, group) == SSH_FX_OK)
        return {};
    return std::unexpected(Error(_impl_ptr->sftp_session_ptr,
                                 "could not change owner of '{}' ({}:{}): {}",
                                 path,
                                 owner,
                                 group,
                                 sftp_error_str(_impl_ptr->sftp_session_ptr)));
}

int Sftp::version()
{
    return sftp_server_version(_impl_ptr->sftp_session_ptr);
}

std::vector<SftpExtension> Sftp::extensions()
{
    std::vector<SftpExtension> ret;
    int extensions_nb = sftp_extensions_get_count(_impl_ptr->sftp_session_ptr);
    int i = 0;
    while (i < extensions_nb)
    {
        const char *extname = sftp_extensions_get_name(_impl_ptr->sftp_session_ptr, i);
        const char *data = sftp_extensions_get_data(_impl_ptr->sftp_session_ptr, i);
        SftpExtension ext;
        ext.name = extname;
        ext.data = data;
        ret.emplace_back(std::move(ext));
        ++i;
    }
    return ret;
}

SftpAttribute::SftpAttribute(std::string_view name, uint8_t type, size_t size)
{
    _name = name;
    _size = size;
    _type = type;
}

SftpAttribute::~SftpAttribute() = default;

const std::string & SftpAttribute::name() const
{
    return _name;
}

uint64_t SftpAttribute::size() const
{
    return _size;
}

bool SftpAttribute::is_dir() const
{
    return _type == SSH_FILEXFER_TYPE_DIRECTORY;
}

bool SftpAttribute::is_file() const
{
    return _type == SSH_FILEXFER_TYPE_REGULAR;
}

bool SftpAttribute::is_link() const
{
    return _type == SSH_FILEXFER_TYPE_SYMLINK;
}

uint8_t SftpAttribute::type() const
{
    return _type;
}

} // namespace sihd::ssh
