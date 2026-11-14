#ifndef __SIHD_SYS_LINUX_DBUS_ITER_HPP__
#define __SIHD_SYS_LINUX_DBUS_ITER_HPP__

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <sihd/sys/linux/dbus/Message.hpp>
#include <sihd/sys/linux/dbus/UnixFd.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::linux_::dbus
{

// forward-only reader over a message body or container, valid while its message lives unchanged
class Connection::Message::Iter
{
    public:
        class Cursor;
        template <BasicType Key>
        class Entries;

        explicit Iter(const Message & message);
        Iter(Iter && other);
        Iter & operator=(Iter && other);
        ~Iter();

        Iter(const Iter &) = delete;
        Iter & operator=(const Iter &) = delete;

        // dbus type code of the current element, '\0' past the end
        char type() const;
        std::string signature() const;
        bool next();

        // from the current element to the end: the range moves this iter
        Cursor begin();
        std::default_sentinel_t end() const;

        // std::string reads strings, object paths and signatures
        template <BasicType T>
        sihd::util::Result<T> get() const;
        template <BasicType T>
        sihd::util::Result<std::vector<T>> get_array() const;
        // a{Kv} holding an ay in every variant
        template <BasicType Key>
        sihd::util::Result<std::map<Key, std::vector<uint8_t>>> get_byte_map() const;
        sihd::util::Result<UnixFd> get_unix_fd() const;
        // a dict walked by range-for, each value unwrapped from its variant
        template <BasicType Key>
        sihd::util::Result<Entries<Key>> entries() const;
        // the basic values from the current element on, this iter does not move
        template <BasicType... T>
        sihd::util::Result<std::tuple<T...>> read() const;

        sihd::util::Result<Iter> recurse() const;

    private:
        struct Impl;

        explicit Iter(std::unique_ptr<Impl> impl);

        sihd::util::Result<Iter> duplicate() const;
        template <BasicType First, BasicType... Rest>
        sihd::util::Result<std::tuple<First, Rest...>> take(std::size_t index, std::size_t count);

        std::unique_ptr<Impl> _impl;
};

using Iter = Connection::Message::Iter;

class Iter::Cursor
{
    public:
        explicit Cursor(Iter & iter);

        Iter & operator*() const;
        Cursor & operator++();
        bool operator==(std::default_sentinel_t) const;

    private:
        Iter *_iter;
};

// Iter must be complete: the entries hold them inline
template <BasicType Key>
class Iter::Entries
{
    public:
        class Cursor
        {
            public:
                explicit Cursor(Entries & entries);

                std::pair<Key, Iter> & operator*() const;
                Cursor & operator++();
                bool operator==(std::default_sentinel_t) const;

            private:
                Entries *_entries;
        };

        // the items of a dict whose signature Iter::entries() checked
        explicit Entries(Iter items);

        Cursor begin();
        std::default_sentinel_t end() const;

    private:
        void decode();

        Iter _items;
        std::pair<Key, Iter> _current;
};

template <BasicType... T>
sihd::util::Result<std::tuple<T...>> Iter::read() const
{
    auto cursor = this->duplicate();
    SIHD_UNEXPECTED_RETURN(cursor);
    return cursor->take<T...>(0, sizeof...(T));
}

template <BasicType First, BasicType... Rest>
sihd::util::Result<std::tuple<First, Rest...>> Iter::take(std::size_t index, std::size_t count)
{
    if (this->type() == '\0')
        return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::invalid_argument,
                                                 "dbus message has {} arguments, expected {}",
                                                 index,
                                                 count));
    auto first = this->get<First>();
    SIHD_UNEXPECTED_RETURN_CTX(first, "argument {}", index);
    if constexpr (sizeof...(Rest) == 0)
        return std::tuple<First>(std::move(*first));
    else
    {
        this->next();
        auto rest = this->take<Rest...>(index + 1, count);
        SIHD_UNEXPECTED_RETURN(rest);
        return std::tuple_cat(std::tuple<First>(std::move(*first)), std::move(*rest));
    }
}

} // namespace sihd::sys::linux_::dbus

#endif
