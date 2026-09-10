#include <sihd/sys/MappedFile.hpp>

// create()/open_read_only()/open_read_write()/clear()/sync() live in src/linux|windows/MappedFile.cpp

namespace sihd::sys
{

MappedFile::MappedFile(): _fd(-1), _mapping(-1), _size(0), _addr(nullptr), _read_only(false) {}

MappedFile::MappedFile(MappedFile && other):
    _fd(other._fd),
    _mapping(other._mapping),
    _size(other._size),
    _addr(other._addr),
    _read_only(other._read_only),
    _path(std::move(other._path))
{
    other._fd = -1;
    other._mapping = -1;
    other._size = 0;
    other._addr = nullptr;
    other._read_only = false;
    other._path.clear();
}

MappedFile & MappedFile::operator=(MappedFile && other)
{
    if (this != &other)
    {
        this->clear();
        _fd = other._fd;
        _mapping = other._mapping;
        _size = other._size;
        _addr = other._addr;
        _read_only = other._read_only;
        _path = std::move(other._path);
        other._fd = -1;
        other._mapping = -1;
        other._size = 0;
        other._addr = nullptr;
        other._read_only = false;
        other._path.clear();
    }
    return *this;
}

MappedFile::~MappedFile()
{
    this->clear();
}

} // namespace sihd::sys
