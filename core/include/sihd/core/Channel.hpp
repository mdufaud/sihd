#ifndef __SIHD_CORE_CHANNEL_HPP__
# define __SIHD_CORE_CHANNEL_HPP__

# include <sihd/util/Array.hpp>
# include <sihd/util/Named.hpp>
# include <sihd/util/Observable.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/time.hpp>
# include <sihd/util/Clocks.hpp>
# include <mutex>

namespace sihd::core
{

LOGGER;
using namespace sihd::util;

class Channel:  public Named,
                public Observable<Channel>
{
    public:
        Channel(const std::string & name,
                const std::string & type,
                size_t size,
                Node *parent = nullptr);
        Channel(const std::string & name,
                const std::string & type,
                Node *parent = nullptr);
        Channel(const std::string & name,
                sihd::util::Type type,
                size_t size,
                Node *parent = nullptr);
        Channel(const std::string & name,
                sihd::util::Type type,
                Node *parent = nullptr);
        virtual ~Channel();

        // "name=CHANNEL_NAME;type=CHANNEL_TYPE;size=CHANNEL_SIZE"
        static Channel *build(const std::string & configuration);

        IArray *arr() { return _array_ptr; }

        std::time_t timestamp();
        void set_clock(sihd::util::IClock *clock);
        void notify();

        static IClock *get_default_clock() { return _default_channel_clock_ptr; }

        // write and notify only if a change happened
        void set_write_on_change(bool activate) { _write_change_only = activate; }

        template <typename T>
        Array<T> *arr()
        {
            return ArrayUtil::cast_array<T>(_array_ptr);
        }

        bool copy_to(IArray & arr);

        template <typename T>
        T   read(size_t idx)
        {
            std::lock_guard lock(_arr_mutex);
            return ArrayUtil::read_array<T>(_array_ptr, idx);
        }

        bool write(const IArray & arr);

        template <typename T>
        bool write(size_t idx, T value)
        {
            Array<T> *arr = ArrayUtil::cast_array<T>(_array_ptr);
            bool ret = arr != nullptr;
            if (ret)
            {
                std::lock_guard lock(_arr_mutex);
                if (_write_change_only && arr->at(idx) == value)
                    return true;
                arr->set(idx, value);
            }
            else
            {
                LOG(error, "Channel: wrong type for writing "
                        << _array_ptr->data_type_to_string() << " != "
                        << Datatype::type_to_string<T>())
            }
            if (ret)
            {
                _timestamp = _clock_ptr->now();
                this->notify();
            }
            return ret;
        }

    protected:
        virtual void _init(Type type, size_t size);

        static IClock *_default_channel_clock_ptr;
    
    private:
        IClock *_clock_ptr;
        std::time_t _timestamp;

        IArray *_array_ptr;
        std::mutex _arr_mutex;

        bool _notifying;
        std::mutex _notify_mutex;

        bool _write_change_only;
};

}

#endif 