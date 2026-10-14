#ifndef __SIHD_SYS_MAPPEDFILE_HPP__
#define __SIHD_SYS_MAPPEDFILE_HPP__

#include <sys/stat.h> // mode_t

#include <string>
#include <type_traits>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/traits.hpp>

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
        sihd::util::Result<void> create(const std::string & path, size_t size, mode_t mode = 0600);

        // mapping an empty file fails
        sihd::util::Result<void> open_read_only(const std::string & path);
        sihd::util::Result<void> open_read_write(const std::string & path);

        sihd::util::Result<void> clear();
        sihd::util::Result<void> sync(bool async = false);

        template <class Self>
        auto data(this Self && self) -> sihd::util::traits::const_prop_ptr<Self, void>
        {
            return self._addr;
        }

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
