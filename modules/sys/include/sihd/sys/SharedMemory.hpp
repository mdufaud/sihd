#ifndef __SIHD_SYS_SHAREDMEMORY_HPP__
#define __SIHD_SYS_SHAREDMEMORY_HPP__

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

class SharedMemory
{
    public:
        static constexpr bool supported = sihd::util::build::is_windows
            || (sihd::util::build::is_unix && !sihd::util::build::is_android
                && !sihd::util::build::is_emscripten);

        SharedMemory();
        virtual ~SharedMemory();

        bool create(std::string_view id, size_t size, mode_t mode = 0600);

        bool attach(std::string_view id, size_t size, mode_t mode = 0600);
        bool attach_read_only(std::string_view id, size_t size, mode_t mode = 0400);

        bool clear();

        void *data() { return _addr; }

        int fd() const { return _fd; }
        size_t size() const { return _size; }
        const void *cdata() const { return _addr; }
        const std::string & id() const { return _id; }

        bool creator() const { return _created; }

    protected:

    private:
        int _fd;
        size_t _size;
        void *_addr;
        bool _created;
        std::string _id;
};

} // namespace sihd::sys

#if defined(__SIHD_EMSCRIPTEN__)
# undef mode_t
#endif

#endif