#ifndef __SIHD_CORE_CHANNEL_HPP__
#define __SIHD_CORE_CHANNEL_HPP__

#include <atomic>
#include <mutex>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/array_utils.hpp>

namespace sihd::core
{

class Channel: public sihd::util::Named,
               public sihd::util::Observable<Channel>
{
    public:
        Channel(const std::string & name, sihd::util::Type type, size_t size, sihd::util::Node *parent = nullptr);
        Channel(const std::string & name, sihd::util::Type type, sihd::util::Node *parent = nullptr);
        Channel(const std::string & name, std::string_view type, size_t size, sihd::util::Node *parent = nullptr);
        Channel(const std::string & name, std::string_view type, sihd::util::Node *parent = nullptr);
        virtual ~Channel();

        static sihd::util::IClock *default_clock() { return _default_channel_clock_ptr; }

        // "name=CHANNEL_NAME;type=CHANNEL_TYPE;size=CHANNEL_SIZE"
        static Channel *build(std::string_view configuration);

        // write and notify only if a change happened
        void set_write_on_change(bool activate) { _write_change_only = activate; }
        void set_clock(sihd::util::IClock *clock);

        // Named
        virtual std::string description() const override;

        uint8_t *data() const { return _array_ptr->buf(); }
        const sihd::util::IArray *array() const { return _array_ptr; }

        size_t size() const { return _array_ptr->size(); }
        size_t byte_size() const { return _array_ptr->byte_size(); }
        size_t byte_index(size_t idx) const { return _array_ptr->byte_index(idx); }

        size_t data_size() const { return _array_ptr->data_size(); }
        sihd::util::Type data_type() const { return _array_ptr->data_type(); }

        bool is_same_type(const Channel *other) const { return _array_ptr->is_same_type(*other->array()); }

        template <typename T>
        bool is_same_type() const
        {
            return sihd::util::Types::is_same<T>(_array_ptr->data_type());
        }

        // get last write timestamp (thread safe)
        sihd::util::Timestamp timestamp() const;

        // write internal timestamp with clock
        void do_timestamp();

        // notifies all observers and prevent writing inside notification thread
        void notify();

        // copy internal array into arr
        bool copy_to(sihd::util::IArray & arr, size_t byte_offset = 0) const;

        // utility for reading

        template <typename T>
        bool read_into(size_t idx, T & val) const
        {
            std::lock_guard lock(_arr_mutex);
            return sihd::util::array_utils::read_into<T>(_array_ptr, idx, val);
        }

        template <typename T>
        T read(size_t idx) const
        {
            std::lock_guard lock(_arr_mutex);
            return sihd::util::array_utils::read<T>(_array_ptr, idx);
        }

        // copy arr to internal array
        bool write(const sihd::util::ArrByteView & arr, size_t byte_offset = 0);
        bool write(const Channel & other);

        // utility for writing
        template <typename T>
        bool write(size_t idx, T value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            return this->write({(const int8_t *)&value, sizeof(T)}, _array_ptr->byte_index(idx));
        }

    protected:
        static sihd::util::IClock *_default_channel_clock_ptr;

    private:
        sihd::util::IClock *_clock_ptr;
        time_t _timestamp;

        sihd::util::IArray *_array_ptr;
        mutable std::mutex _arr_mutex;

        std::atomic<bool> _notifying;
        mutable std::mutex _notify_mutex;

        bool _write_change_only;
};

} // namespace sihd::core

#endif
