#ifndef __SIHD_UTIL_SHAREDMEMORY_HPP__
# define __SIHD_UTIL_SHAREDMEMORY_HPP__

# include <sihd/util/platform.hpp>
# include <string>

namespace sihd::util
{

class SharedMemory
{
    public:
        SharedMemory();
        virtual ~SharedMemory();

        bool create(const std::string & id, size_t size, mode_t mode = 0600);
        bool create_read_only(const std::string & id, size_t size, mode_t mode = 0600);

        bool attach(const std::string & id, size_t size, mode_t mode = 0600);
        bool attach_read_only(const std::string & id, size_t size, mode_t mode = 0400);

        bool clear();

        void *data() { return _addr; }

        int fd() const { return _fd; }
        size_t size() const { return _size; }
        const void *cdata() const { return _addr; }
        const std::string & id() const { return _id; }
        bool attached() const { return _created == false; }

    protected:

    private:
#if !defined(__SIHD_WINDOWS__)
        bool _create(const std::string & id, size_t size, mode_t mode, int shm_flags, int mmap_flags);
        bool _attach(const std::string & id, size_t size, mode_t mode, int shm_flags, int mmap_flags);
#endif
        int _fd;
        size_t _size;
        void *_addr;
        bool _created;
        std::string _id;
};

}

#endif