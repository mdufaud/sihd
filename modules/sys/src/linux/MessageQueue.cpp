#include <fcntl.h> // O_*
#include <mqueue.h>

#include <cerrno>
#include <vector>

#include <sihd/sys/MessageQueue.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>

#include "internal/deadline.hpp"

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

#if defined(__SIHD_UNIX__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_APPLE__) && !defined(__SIHD_EMSCRIPTEN__)

namespace
{

std::string mq_name(std::string_view name)
{
    if (name.empty() || name.front() == '/')
        return std::string(name);
    return "/" + std::string(name);
}

mqd_t mq_handle(intptr_t mq)
{
    return static_cast<mqd_t>(mq);
}

} // namespace

sihd::util::Result<void>
    MessageQueue::create(std::string_view name, size_t max_messages, size_t max_message_size, mode_t mode)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    struct mq_attr attr = {};
    attr.mq_maxmsg = static_cast<long>(max_messages);
    attr.mq_msgsize = static_cast<long>(max_message_size);

    mqd_t mq = mq_open(mq_name(name).c_str(), O_CREAT | O_EXCL | O_RDWR, mode, &attr);
    if (mq == static_cast<mqd_t>(-1))
        return std::unexpected(Error::from_errno("could not mq_open '{}'", name));

    _mq = mq;
    _created = true;
    _msg_size = max_message_size;
    _name = name;
    return {};
}

sihd::util::Result<void> MessageQueue::open(std::string_view name)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    mqd_t mq = mq_open(mq_name(name).c_str(), O_RDWR);
    if (mq == static_cast<mqd_t>(-1))
        return std::unexpected(Error::from_errno("could not mq_open '{}'", name));

    struct mq_attr attr = {};
    if (mq_getattr(mq, &attr) == -1)
    {
        auto error = Error::from_errno("could not mq_getattr '{}'", name);
        mq_close(mq);
        return std::unexpected(std::move(error));
    }

    _mq = mq;
    _created = false;
    _msg_size = static_cast<size_t>(attr.mq_msgsize);
    _name = name;
    return {};
}

sihd::util::Result<void> MessageQueue::unlink(std::string_view name)
{
    if (mq_unlink(mq_name(name).c_str()) == -1)
        return std::unexpected(Error::from_errno("could not mq_unlink '{}'", name));
    return {};
}

sihd::util::Result<void> MessageQueue::clear()
{
    bool ret = true;
    if (_mq != -1)
    {
        if (mq_close(mq_handle(_mq)) == -1)
            ret = false;
        _mq = -1;
        _msg_size = 0;
        _name.clear();
    }
    _created = false;
    if (!ret)
        return std::unexpected(Error(io_error, "could not clear message queue"));
    return {};
}

sihd::util::Result<void> MessageQueue::send(ArrByteView data, unsigned priority)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    while (mq_send(mq_handle(_mq), reinterpret_cast<const char *>(data.data()), data.byte_size(), priority) == -1)
    {
        if (errno != EINTR)
            return std::unexpected(Error::from_errno("could not mq_send '{}'", _name));
    }
    return {};
}

sihd::util::Result<void> MessageQueue::try_send(ArrByteView data, unsigned priority)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    struct mq_attr attr = {};
    if (mq_getattr(mq_handle(_mq), &attr) == -1)
        return std::unexpected(Error::from_errno("could not mq_getattr '{}'", _name));
    const bool was_nonblocking = attr.mq_flags & O_NONBLOCK;

    if (!was_nonblocking)
    {
        auto nonblocking = this->set_nonblocking(true);
        SIHD_UNEXPECTED_RETURN(nonblocking);
    }

    auto sent = this->send(data, priority);

    if (!was_nonblocking)
    {
        auto restored = this->set_nonblocking(false);
        if (!restored)
            return restored;
    }
    return sent;
}

sihd::util::Result<void> MessageQueue::timed_send(ArrByteView data, Duration timeout, unsigned priority)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    const Timestamp begin = internal::steady_now();

    while (true)
    {
        auto deadline = internal::realtime_deadline(begin, timeout);
        if (!deadline)
            return std::unexpected(Error(ErrorCode::timeout, "could not mq_send '{}': timed out", _name));

        if (mq_timedsend(mq_handle(_mq),
                         reinterpret_cast<const char *>(data.data()),
                         data.byte_size(),
                         priority,
                         &*deadline)
            == 0)
            return {};
        if (errno == ETIMEDOUT)
            return std::unexpected(Error(ErrorCode::timeout, "could not mq_send '{}': timed out", _name));
        if (errno != EINTR)
            return std::unexpected(Error::from_errno("could not mq_send '{}'", _name));
    }
}

sihd::util::Result<std::string> MessageQueue::receive(unsigned *priority)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    std::vector<char> buffer(_msg_size);
    unsigned prio = 0;
    ssize_t size;
    while ((size = mq_receive(mq_handle(_mq), buffer.data(), buffer.size(), &prio)) == -1)
    {
        if (errno != EINTR)
            return std::unexpected(Error::from_errno("could not mq_receive '{}'", _name));
    }
    if (priority != nullptr)
        *priority = prio;
    return std::string(buffer.data(), static_cast<size_t>(size));
}

std::optional<std::string> MessageQueue::try_receive(unsigned *priority)
{
    if (_mq == -1)
        return std::nullopt;

    struct mq_attr attr = {};
    if (mq_getattr(mq_handle(_mq), &attr) == -1)
        return std::nullopt;
    const bool was_nonblocking = attr.mq_flags & O_NONBLOCK;

    if (!was_nonblocking && !this->set_nonblocking(true))
        return std::nullopt;

    std::vector<char> buffer(_msg_size);
    unsigned prio = 0;
    ssize_t size;
    while ((size = mq_receive(mq_handle(_mq), buffer.data(), buffer.size(), &prio)) == -1)
    {
        if (errno != EINTR)
            break;
    }
    std::optional<std::string> ret;
    if (size >= 0)
    {
        if (priority != nullptr)
            *priority = prio;
        ret = std::string(buffer.data(), static_cast<size_t>(size));
    }

    if (!was_nonblocking)
        (void)this->set_nonblocking(false);

    return ret;
}

sihd::util::Result<std::string> MessageQueue::timed_receive(Duration timeout, unsigned *priority)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    const Timestamp begin = internal::steady_now();
    std::vector<char> buffer(_msg_size);

    while (true)
    {
        auto deadline = internal::realtime_deadline(begin, timeout);
        if (!deadline)
            return std::unexpected(Error(ErrorCode::timeout, "could not mq_receive '{}': timed out", _name));

        unsigned prio = 0;
        ssize_t size = mq_timedreceive(mq_handle(_mq), buffer.data(), buffer.size(), &prio, &*deadline);
        if (size >= 0)
        {
            if (priority != nullptr)
                *priority = prio;
            return std::string(buffer.data(), static_cast<size_t>(size));
        }
        if (errno == ETIMEDOUT)
            return std::unexpected(Error(ErrorCode::timeout, "could not mq_receive '{}': timed out", _name));
        if (errno != EINTR)
            return std::unexpected(Error::from_errno("could not mq_receive '{}'", _name));
    }
}

sihd::util::Result<MessageQueue::Attributes> MessageQueue::attributes() const
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    struct mq_attr attr = {};
    if (mq_getattr(mq_handle(_mq), &attr) == -1)
        return std::unexpected(Error::from_errno("could not mq_getattr '{}'", _name));
    return Attributes {attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs};
}

sihd::util::Result<void> MessageQueue::set_nonblocking(bool active)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    mqd_t mq = mq_handle(_mq);
    struct mq_attr attr = {};
    if (mq_getattr(mq, &attr) == -1)
        return std::unexpected(Error::from_errno("could not mq_getattr '{}'", _name));
    if (active)
        attr.mq_flags |= O_NONBLOCK;
    else
        attr.mq_flags &= ~static_cast<long>(O_NONBLOCK);
    if (mq_setattr(mq, &attr, nullptr) == -1)
        return std::unexpected(Error::from_errno("could not mq_setattr '{}'", _name));
    return {};
}

#else

// no POSIX message queues (android bionic, apple has no mq_*, emscripten)

sihd::util::Result<void> MessageQueue::create(std::string_view, size_t, size_t, mode_t)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

sihd::util::Result<void> MessageQueue::open(std::string_view)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

sihd::util::Result<void> MessageQueue::unlink(std::string_view)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

sihd::util::Result<void> MessageQueue::clear()
{
    return {};
}

sihd::util::Result<void> MessageQueue::send(ArrByteView, unsigned)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

sihd::util::Result<void> MessageQueue::try_send(ArrByteView, unsigned)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

sihd::util::Result<void> MessageQueue::timed_send(ArrByteView, Duration, unsigned)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

sihd::util::Result<std::string> MessageQueue::receive(unsigned *)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::optional<std::string> MessageQueue::try_receive(unsigned *)
{
    return std::nullopt;
}

sihd::util::Result<std::string> MessageQueue::timed_receive(Duration, unsigned *)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

sihd::util::Result<MessageQueue::Attributes> MessageQueue::attributes() const
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

sihd::util::Result<void> MessageQueue::set_nonblocking(bool)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

#endif

} // namespace sihd::sys
