#ifndef __SIHD_UTIL_SYNCHRONIZER_HPP__
# define __SIHD_UTIL_SYNCHRONIZER_HPP__

# include <sihd/util/Waitable.hpp>
# include <atomic>

namespace sihd::util
{

class Synchronizer
{
    public:
        Synchronizer();
        virtual ~Synchronizer();

        void init_sync(int32_t total);
        void sync();
        void reset();

        int32_t to_sync() const { return _wanted_count.load(); }

    protected:

    private:
        void _unlock_all();

        std::atomic<int32_t> _sync_count;
        std::atomic<int32_t> _wanted_count;
        Waitable _waitable;
};

}

#endif