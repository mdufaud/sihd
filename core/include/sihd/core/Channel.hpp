#ifndef __SIHD_CORE_CHANNEL_HPP__
# define __SIHD_CORE_CHANNEL_HPP__

# include <sihd/util/Array.hpp>
# include <sihd/util/Named.hpp>
# include <sihd/util/Observable.hpp>
# include <sihd/util/Logger.hpp>
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
        virtual ~Channel();

        IArray  *arr() { return _array_ptr; }

        template <typename T>
        Array<T>    *arr()
        {
            return ArrayUtil::cast_array<T>(_array_ptr);
        }

        bool    copy_to(IArray *arr);

        template <typename T>
        T   read(size_t idx)
        {
            std::lock_guard lock(_arr_mutex);
            return ArrayUtil::read_array<T>(_array_ptr, idx);
        }

        bool    write(IArray *arr);

        template <typename T>
        bool    write(size_t idx, T value)
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
                this->notify();
            return ret;
        }

        void    notify();

        // write and notify only if a change happened
        void    set_write_on_change(bool activate) { _write_change_only = activate; }

    protected:
        virtual void    _init(const std::string & type, size_t size);
    
    private:
        IArray  *_array_ptr;
        bool    _notifying;
        bool    _write_change_only;
        std::mutex  _arr_mutex;
        std::mutex  _notify_mutex;
};

}

#endif 