#ifndef __SIHD_CORE_CHANNEL_HPP__
#define __SIHD_CORE_CHANNEL_HPP__

#include <atomic>
#include <mutex>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Slice.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Value.hpp>
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

        static sihd::util::IClock *default_clock();

        // "name=CHANNEL_NAME;type=CHANNEL_TYPE;size=CHANNEL_SIZE"
        static Channel *build(std::string_view configuration);

        void set_write_on_change(bool activate);
        void set_resizable(bool activate);
        void set_clock(sihd::util::IClock *clock);

        virtual std::string description() const override;

        // unlocked views: stable inside a notification or without a concurrent writer
        uint8_t *data() const;
        sihd::util::ArrUByteView arrview() const;
        const sihd::util::IArray *array() const;

        size_t size() const;
        size_t byte_size() const;
        size_t byte_index(size_t idx) const;

        bool resizable() const;
        size_t capacity() const;
        size_t byte_capacity() const;
        // refused while notifying, like every write
        bool reserve(size_t capacity);
        bool resize(size_t size);

        size_t data_size() const;
        sihd::util::Type data_type() const;

        bool is_same_type(const Channel *other) const;

        template <typename T>
        bool is_same_type() const
        {
            return sihd::util::type::is_same<T>(_array_ptr->data_type());
        }

        sihd::util::Timestamp timestamp() const;

        // refused while notifying, like every write
        bool notify();

        bool copy_to(sihd::util::IArray & arr, sihd::util::Timestamp *timestamp = nullptr) const;
        bool copy_to(sihd::util::IArray & arr,
                     sihd::util::Slice slice,
                     sihd::util::Timestamp *timestamp = nullptr) const;
        bool copy_to_bytes(sihd::util::IArray & arr,
                           sihd::util::Slice byte_slice,
                           sihd::util::Timestamp *timestamp = nullptr) const;
        bool copy_resized_to(sihd::util::IArray & arr, sihd::util::Timestamp *timestamp = nullptr) const;
        sihd::util::IArrayUnique clone_array() const;

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

        // empty if idx is out of range
        sihd::util::Value value_at(size_t idx) const;

        // a resizable channel takes the size of arr
        bool write(const sihd::util::ArrByteView & arr);
        // a resizable channel grows to fit the write, never shrinks
        bool write(const sihd::util::ArrByteView & arr, size_t byte_offset);
        bool write(const Channel & other);

        template <typename T>
        bool write(size_t idx, T value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            return this->write({(const int8_t *)&value, sizeof(T)}, _array_ptr->byte_index(idx));
        }

    protected:
        // app-wide default clock for every channel
        static sihd::util::IClock *_default_channel_clock_ptr;

    private:
        bool _write(const sihd::util::ArrByteView & arr, size_t byte_offset, bool fit_size);
        // owns nothing when another mutation or a notification is in progress
        std::unique_lock<std::recursive_mutex> _lock_mutation(const char *operation);
        void _notify();
        void _do_timestamp();

        sihd::util::IClock *_clock_ptr;
        sihd::util::Timestamp _timestamp;

        sihd::util::IArray *_array_ptr;
        mutable std::mutex _arr_mutex;

        // recursive: an observer mutating its channel relocks it, then sees _notifying
        std::recursive_mutex _notify_mutex;
        bool _notifying;

        bool _write_change_only;
        bool _resizable;
};

} // namespace sihd::core

#endif
