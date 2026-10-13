#ifndef __SIHD_SYS_MESSAGEQUEUE_HPP__
#define __SIHD_SYS_MESSAGEQUEUE_HPP__

#include <sys/stat.h> // mode_t

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/sys/platform.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_EMSCRIPTEN__)
# define mode_t unsigned int
#endif

namespace sihd::sys
{

class MessageQueue
{
    public:
        struct Attributes
        {
                long max_messages;
                long max_message_size;
                long current_messages;
        };

        static constexpr bool supported = sihd::util::build::is_windows
                                          || (sihd::util::build::is_unix && !sihd::util::build::is_android
                                              && !sihd::util::build::is_apple && !sihd::util::build::is_emscripten);

        MessageQueue();
        MessageQueue(MessageQueue && other);
        MessageQueue & operator=(MessageQueue && other);
        ~MessageQueue();

        MessageQueue(const MessageQueue &) = delete;
        MessageQueue & operator=(const MessageQueue &) = delete;

        sihd::util::Result<void>
            create(std::string_view name, size_t max_messages, size_t max_message_size, mode_t mode = 0600);

        sihd::util::Result<void> open(std::string_view name);

        static sihd::util::Result<void> unlink(std::string_view name);

        sihd::util::Result<void> unlink();

        sihd::util::Result<void> clear();

        sihd::util::Result<void> send(sihd::util::ArrByteView data, unsigned priority = 0);
        sihd::util::Result<void> try_send(sihd::util::ArrByteView data, unsigned priority = 0);
        sihd::util::Result<void>
            timed_send(sihd::util::ArrByteView data, sihd::util::Duration timeout, unsigned priority = 0);

        sihd::util::Result<std::string> receive(unsigned *priority = nullptr);
        std::optional<std::string> try_receive(unsigned *priority = nullptr);
        sihd::util::Result<std::string> timed_receive(sihd::util::Duration timeout, unsigned *priority = nullptr);

        sihd::util::Result<Attributes> attributes() const;

        sihd::util::Result<void> set_nonblocking(bool active);

    protected:

    private:
        // -1 = closed: a 0 mqd_t is a valid descriptor, nullptr would conflate it
        intptr_t _mq;
        bool _created;
        bool _connected;
        bool _nonblocking;
        size_t _msg_size;
        std::string _name;
};

} // namespace sihd::sys

#if defined(__SIHD_EMSCRIPTEN__)
# undef mode_t
#endif

#endif
