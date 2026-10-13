#include <sihd/sys/MessageQueue.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

// create()/open()/clear()/unlink(name) and the send/receive family live in src/linux|windows/MessageQueue.cpp

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

MessageQueue::MessageQueue(): _mq(-1), _created(false), _connected(false), _nonblocking(false), _msg_size(0) {}

MessageQueue::MessageQueue(MessageQueue && other):
    _mq(other._mq),
    _created(other._created),
    _connected(other._connected),
    _nonblocking(other._nonblocking),
    _msg_size(other._msg_size),
    _name(std::move(other._name))
{
    other._mq = -1;
    other._created = false;
    other._connected = false;
    other._nonblocking = false;
    other._msg_size = 0;
    other._name.clear();
}

MessageQueue & MessageQueue::operator=(MessageQueue && other)
{
    if (this != &other)
    {
        (void)this->clear();
        _mq = other._mq;
        _created = other._created;
        _connected = other._connected;
        _nonblocking = other._nonblocking;
        _msg_size = other._msg_size;
        _name = std::move(other._name);
        other._mq = -1;
        other._created = false;
        other._connected = false;
        other._nonblocking = false;
        other._msg_size = 0;
        other._name.clear();
    }
    return *this;
}

MessageQueue::~MessageQueue()
{
    (void)this->clear();
}

std::expected<void, Error> MessageQueue::unlink()
{
    if (_name.empty())
        return std::unexpected(Error(not_initialized, "message queue is not open"));
    return unlink(_name);
}

} // namespace sihd::sys
