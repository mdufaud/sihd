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

class Channel:  public sihd::util::Named,
                public sihd::util::Observable<Channel>
{
    public:
        Channel(const std::string & name,
                const std::string & type,
                size_t size,
                sihd::util::Node *parent = nullptr);
        Channel(const std::string & name,
                const std::string & type,
                sihd::util::Node *parent = nullptr);
        Channel(const std::string & name,
                sihd::util::Type type,
                size_t size,
                sihd::util::Node *parent = nullptr);
        Channel(const std::string & name,
                sihd::util::Type type,
                sihd::util::Node *parent = nullptr);
        virtual ~Channel();

        static sihd::util::IClock *get_default_clock() { return _default_channel_clock_ptr; }

        // "name=CHANNEL_NAME;type=CHANNEL_TYPE;size=CHANNEL_SIZE"
        static Channel *build(const std::string & configuration);

        void set_clock(sihd::util::IClock *clock);
        // get last write timestamp (thread safe)
        std::time_t timestamp();
        // get last write timestamp (not thread safe)
        std::time_t ctimestamp() const { return _timestamp; };

        // notifies all observers and prevent writing inside notification thread
        void notify();

        // write and notify only if a change happened
        void set_write_on_change(bool activate) { _write_change_only = activate; }

        sihd::util::IArray *array() { return _array_ptr; }
        const sihd::util::IArray *carray() const { return _array_ptr; }

        template <typename T>
        sihd::util::Array<T> *array()
        {
            return sihd::util::ArrayUtil::cast_array<T>(_array_ptr);
        }

        // copy internal array into arr
        bool copy_to(sihd::util::IArray & arr);

        // utility for reading (dynamic cast is slow)
        template <typename T>
        T   read(size_t idx)
        {
            std::lock_guard lock(_arr_mutex);
            return sihd::util::ArrayUtil::read_array<T>(_array_ptr, idx);
        }

        // copy arr to internal array
        bool write(const sihd::util::IArray & arr);

        // utility for writing (dynamic cast is slow)
        template <typename T>
        bool write(size_t idx, T value)
        {
            if (_notifying)
            {
                LOG(warning, "Channel: cannot write while notifying");
                return false;
            }
            sihd::util::Array<T> *arr = sihd::util::ArrayUtil::cast_array<T>(_array_ptr);
            bool ret = arr != nullptr;
            if (ret)
            {
                std::lock_guard lock(_arr_mutex);
                if (_write_change_only && arr->at(idx) == value)
                    return true;
                arr->set(idx, value);
                _timestamp = _clock_ptr->now();
            }
            else
            {
                LOG(error, "Channel: wrong type for writing "
                        << _array_ptr->data_type_to_string() << " != "
                        << sihd::util::Datatype::type_to_string<T>())
            }
            if (ret)
                this->notify();
            return ret;
        }

    protected:
        virtual void _init(sihd::util::Type type, size_t size);

        static sihd::util::IClock *_default_channel_clock_ptr;
    
    private:
        sihd::util::IClock *_clock_ptr;
        std::time_t _timestamp;

        sihd::util::IArray *_array_ptr;
        std::mutex _arr_mutex;

        bool _notifying;
        std::mutex _notify_mutex;

        bool _write_change_only;
};

}

#endif 