#ifndef __SIHD_UTIL_SHAREDMEMORY_HPP__
#define __SIHD_UTIL_SHAREDMEMORY_HPP__

#include <sys/stat.h>

#include <string>
#include <string_view>

#include <sihd/util/platform.hpp>

#if defined(__SIHD_EMSCRIPTEN__)
# define mode_t unsigned int
#endif

namespace sihd::util
{

class SharedMemory
{
    public:
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

} // namespace sihd::util

#if defined(__SIHD_EMSCRIPTEN__)
# undef mode_t
#endif

#endif