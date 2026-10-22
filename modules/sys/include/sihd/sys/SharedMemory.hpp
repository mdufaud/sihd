#ifndef __SIHD_SYS_SHAREDMEMORY_HPP__
#define __SIHD_SYS_SHAREDMEMORY_HPP__

#include <string>
#include <type_traits>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/traits.hpp>

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

        sihd::util::Result<void> create(const std::string & id, size_t size, mode_t mode = 0600);

        sihd::util::Result<void> attach(const std::string & id, size_t size, mode_t mode = 0600);
        sihd::util::Result<void> attach_read_only(const std::string & id, size_t size, mode_t mode = 0400);

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

#endif