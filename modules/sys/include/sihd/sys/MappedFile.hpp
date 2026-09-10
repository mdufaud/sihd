#ifndef __SIHD_SYS_MAPPEDFILE_HPP__
#define __SIHD_SYS_MAPPEDFILE_HPP__

#include <sys/stat.h> // mode_t

#include <string>
#include <string_view>

#include <sihd/sys/platform.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_EMSCRIPTEN__)
# define mode_t unsigned int
#endif

namespace sihd::sys
{

// maps a whole file into memory
class MappedFile
{
    public:
        static constexpr bool supported = sihd::util::build::is_windows
                                          || (sihd::util::build::is_unix && !sihd::util::build::is_android
                                              && !sihd::util::build::is_emscripten);

        MappedFile();
        virtual ~MappedFile();

        MappedFile(MappedFile &&);
        MappedFile & operator=(MappedFile &&);

        // truncates/creates 'path' to 'size' bytes and maps it read-write; size must not be 0
        bool create(std::string_view path, size_t size, mode_t mode = 0600);

        // mapping an empty file fails
        bool open_read_only(std::string_view path);
        bool open_read_write(std::string_view path);

        bool clear();
        bool sync(bool async = false);

        void *data() { return _addr; }

        const void *cdata() const { return _addr; }

        int fd() const { return _fd; }
        size_t size() const { return _size; }
        const std::string & path() const { return _path; }
        bool read_only() const { return _read_only; }

    private:
        int _fd;
        // windows keeps the file-mapping handle alongside the file handle
        int _mapping;
        size_t _size;
        void *_addr;
        bool _read_only;
        std::string _path;
};

} // namespace sihd::sys

#if defined(__SIHD_EMSCRIPTEN__)
# undef mode_t
#endif

#endif
