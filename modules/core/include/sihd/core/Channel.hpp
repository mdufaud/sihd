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

        // Named
        virtual std::string description() const override;

        uint8_t *data() const;
        const sihd::util::IArray *array() const;

        size_t size() const;
        size_t byte_size() const;
        size_t byte_index(size_t idx) const;

        bool resizable() const;
        size_t capacity() const;
        size_t byte_capacity() const;
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

        // get last write timestamp (thread safe)
        sihd::util::Timestamp timestamp() const;

        void do_timestamp();

        // notifies all observers and prevent writing inside notification thread
        void notify();

        bool copy_to(sihd::util::IArray & arr, sihd::util::Timestamp *timestamp = nullptr) const;
        bool copy_to(sihd::util::IArray & arr,
                     sihd::util::Slice slice,
                     sihd::util::Timestamp *timestamp = nullptr) const;
        bool copy_to_bytes(sihd::util::IArray & arr,
                           sihd::util::Slice byte_slice,
                           sihd::util::Timestamp *timestamp = nullptr) const;

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

        bool write(const sihd::util::ArrByteView & arr, size_t byte_offset = 0);
        bool write(const Channel & other);

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
        sihd::util::Timestamp _timestamp;

        sihd::util::IArray *_array_ptr;
        mutable std::mutex _arr_mutex;

        std::atomic<bool> _notifying;
        mutable std::mutex _notify_mutex;

        bool _write_change_only;
        bool _resizable;
};

} // namespace sihd::core

#endif
