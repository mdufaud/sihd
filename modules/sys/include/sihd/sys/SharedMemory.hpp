#ifndef __SIHD_SYS_SHAREDMEMORY_HPP__
#define __SIHD_SYS_SHAREDMEMORY_HPP__

#include <sys/stat.h> // mode_t

#include <string>
#include <string_view>
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

class SharedMemory
{
    public:
        static constexpr bool supported = sihd::util::build::is_windows
                                          || (sihd::util::build::is_unix && !sihd::util::build::is_android
                                              && !sihd::util::build::is_emscripten);

        SharedMemory();
        virtual ~SharedMemory();

        SharedMemory(SharedMemory &&);
        SharedMemory & operator=(SharedMemory &&);

        sihd::util::Result<void> create(std::string_view id, size_t size, mode_t mode = 0600);

        sihd::util::Result<void> attach(std::string_view id, size_t size, mode_t mode = 0600);
        sihd::util::Result<void> attach_read_only(std::string_view id, size_t size, mode_t mode = 0400);

        sihd::util::Result<void> clear();

        template <class Self>
        auto data(this Self && self) -> sihd::util::traits::const_prop_ptr<Self, void>
        {
            return self._addr;
        }

        int fd() const { return _fd; }
        size_t size() const { return _size; }
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