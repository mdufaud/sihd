#include <windows.h>

#include <string>

#include <sihd/sys/MessageQueue.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>

#include "internal/wait.hpp"

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using sihd::sys::internal::deadline_ms;

namespace sihd::sys
{

SIHD_LOGGER;

namespace
{

std::string pipe_path(std::string_view name)
{
    return "\\\\.\\pipe\\" + std::string(name);
}

bool is_disconnect(DWORD last_error)
{
    return last_error == ERROR_BROKEN_PIPE || last_error == ERROR_NO_DATA || last_error == ERROR_PIPE_NOT_CONNECTED;
}

// a disconnected peer's writes stay readable: buffered data outlives the connection
bool pipe_has_data(HANDLE handle)
{
    DWORD available = 0;
    return PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr) != FALSE && available > 0;
}

Error closed_error(HANDLE handle, bool created, bool & connected, std::string_view name)
{
    if (created && connected)
        DisconnectNamedPipe(handle);
    connected = false;
    return Error(closed, "message queue '{}': pipe peer is gone", name);
}

Result<void> pipe_connect(HANDLE handle, bool & connected, Duration timeout)
{
    if (connected)
        return {};

    while (true)
    {
        OVERLAPPED overlapped = {};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (overlapped.hEvent == nullptr)
            return std::unexpected(Error(io_error, "could not create pipe event: {}", os::last_error_str()));

        // ERROR_PIPE_CONNECTED: a client connected before the call and is still there
        if (ConnectNamedPipe(handle, &overlapped) == FALSE && GetLastError() != ERROR_PIPE_CONNECTED)
        {
            const DWORD last_error = GetLastError();
            if (last_error != ERROR_IO_PENDING)
            {
                CloseHandle(overlapped.hEvent);
                // a client came and left: its message stays readable, else re-arm for the next one
                if (last_error == ERROR_NO_DATA || last_error == ERROR_BROKEN_PIPE)
                {
                    if (pipe_has_data(handle))
                        break;

                    DisconnectNamedPipe(handle);
                    continue;
                }
                return std::unexpected(
                    Error(io_error, "could not connect pipe: {}", os::error_str(static_cast<int>(last_error))));
            }
            if (WaitForSingleObject(overlapped.hEvent, deadline_ms(timeout)) != WAIT_OBJECT_0)
            {
                CancelIoEx(handle, &overlapped);
                CloseHandle(overlapped.hEvent);
                return std::unexpected(Error(ErrorCode::timeout, "could not connect pipe: timed out"));
            }
        }
        CloseHandle(overlapped.hEvent);
        break;
    }
    connected = true;
    return {};
}

Result<DWORD> pipe_write(HANDLE handle, const void *buffer, DWORD size, Duration timeout)
{
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (overlapped.hEvent == nullptr)
        return std::unexpected(Error(io_error, "could not create pipe event: {}", os::last_error_str()));

    if (WriteFile(handle, buffer, size, nullptr, &overlapped) == FALSE && GetLastError() != ERROR_IO_PENDING)
    {
        const DWORD last_error = GetLastError();
        CloseHandle(overlapped.hEvent);
        if (is_disconnect(last_error))
            return std::unexpected(Error(closed, "could not write pipe: peer is gone"));
        return std::unexpected(
            Error(io_error, "could not write pipe: {}", os::error_str(static_cast<int>(last_error))));
    }

    Result<DWORD> ret = 0;
    if (WaitForSingleObject(overlapped.hEvent, deadline_ms(timeout)) != WAIT_OBJECT_0)
    {
        CancelIoEx(handle, &overlapped);
        DWORD discarded = 0;
        GetOverlappedResult(handle, &overlapped, &discarded, FALSE);
        ret = std::unexpected(Error(ErrorCode::timeout, "could not write pipe: timed out"));
    }
    else
    {
        DWORD transferred = 0;
        if (GetOverlappedResult(handle, &overlapped, &transferred, FALSE) == FALSE)
        {
            const DWORD last_error = GetLastError();
            if (is_disconnect(last_error))
                ret = std::unexpected(Error(closed, "could not write pipe: peer is gone"));
            else
                ret = std::unexpected(
                    Error(io_error, "could not write pipe: {}", os::error_str(static_cast<int>(last_error))));
        }
        else
        {
            ret = transferred;
        }
    }
    CloseHandle(overlapped.hEvent);
    return ret;
}

Result<std::string> pipe_read(HANDLE handle, size_t initial_size, Duration timeout)
{
    std::string buffer(initial_size > 0 ? initial_size : 4096, '\0');
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (overlapped.hEvent == nullptr)
        return std::unexpected(Error(io_error, "could not create pipe event: {}", os::last_error_str()));

    size_t offset = 0;
    Result<std::string> ret = std::unexpected(Error(io_error, "pipe read did not run"));
    bool done = false;
    while (!done)
    {
        if (offset == buffer.size())
            buffer.resize(buffer.size() * 2);

        if (ReadFile(handle, buffer.data() + offset, static_cast<DWORD>(buffer.size() - offset), nullptr, &overlapped)
                == FALSE
            && GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_MORE_DATA)
        {
            const DWORD last_error = GetLastError();
            if (is_disconnect(last_error) && pipe_has_data(handle))
                continue; // the peer exited after writing: its message is still buffered
            if (is_disconnect(last_error))
                ret = std::unexpected(Error(closed, "could not read pipe: peer is gone"));
            else
                ret = std::unexpected(
                    Error(io_error, "could not read pipe: {}", os::error_str(static_cast<int>(last_error))));
            break;
        }

        if (WaitForSingleObject(overlapped.hEvent, deadline_ms(timeout)) != WAIT_OBJECT_0)
        {
            CancelIoEx(handle, &overlapped);
            DWORD discarded = 0;
            GetOverlappedResult(handle, &overlapped, &discarded, FALSE);
            ret = std::unexpected(Error(ErrorCode::timeout, "could not read pipe: timed out"));
            break;
        }

        SetLastError(0);
        DWORD transferred = 0;
        const BOOL result_ok = GetOverlappedResult(handle, &overlapped, &transferred, FALSE);
        const DWORD last_error = GetLastError();

        if (result_ok == FALSE && last_error != ERROR_MORE_DATA)
        {
            if (is_disconnect(last_error) && pipe_has_data(handle))
                continue; // the peer exited after writing: its message is still buffered
            if (is_disconnect(last_error))
                ret = std::unexpected(Error(closed, "could not read pipe: peer is gone"));
            else
                ret = std::unexpected(
                    Error(io_error, "could not read pipe: {}", os::error_str(static_cast<int>(last_error))));
            break;
        }
        offset += transferred;
        // ERROR_MORE_DATA: the message does not fit the remaining buffer, keep reading the same message
        if (last_error == ERROR_MORE_DATA)
        {
            buffer.resize(buffer.size() * 2);
            continue;
        }
        buffer.resize(offset);
        ret = std::move(buffer);
        done = true;
    }
    CloseHandle(overlapped.hEvent);
    return ret;
}

Result<void> queue_send(HANDLE handle,
                        bool created,
                        bool & connected,
                        std::string_view name,
                        Duration wait,
                        ArrByteView data,
                        unsigned priority)
{
    (void)priority; // named pipes carry no priority: delivery stays FIFO

    while (true)
    {
        if (created)
        {
            auto accepted = pipe_connect(handle, connected, wait);
            SIHD_UNEXPECTED_RETURN(accepted);
        }

        auto transferred = pipe_write(handle, data.data(), static_cast<DWORD>(data.byte_size()), wait);
        if (!transferred && transferred.error().code == closed)
        {
            // a reader took nothing and left: re-arm, the next one gets the message
            closed_error(handle, created, connected, name);
            if (wait.get() == 0)
                return std::unexpected(Error(would_block, "could not send '{}': queue is full", name));
            continue;
        }
        SIHD_UNEXPECTED_RETURN(transferred);
        if (*transferred != data.byte_size())
            return std::unexpected(Error(io_error, "could not send '{}': short write", name));
        return {};
    }
}

Result<std::string> queue_receive(HANDLE handle,
                                  bool created,
                                  bool & connected,
                                  std::string_view name,
                                  size_t msg_size,
                                  Duration wait,
                                  unsigned *priority)
{
    if (priority != nullptr)
        *priority = 0;

    while (true)
    {
        if (created)
        {
            auto accepted = pipe_connect(handle, connected, wait);
            SIHD_UNEXPECTED_RETURN(accepted);
        }

        auto message = pipe_read(handle, msg_size, wait);
        if (!message && message.error().code == closed)
        {
            // a writer left nothing behind: re-arm, the next one may write
            closed_error(handle, created, connected, name);
            if (wait.get() == 0)
                return std::unexpected(Error(would_block, "could not receive '{}': queue is empty", name));
            continue;
        }
        SIHD_UNEXPECTED_RETURN(message);
        return message;
    }
}

} // namespace

Result<void> MessageQueue::create(const std::string & name, size_t max_messages, size_t max_message_size, mode_t mode)
{
    (void)mode;

    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    if (max_messages == 0 || max_message_size == 0)
        return std::unexpected(Error(invalid_argument, "could not create message queue '{}': empty bounds", name));
    const size_t buffer_size = max_messages * max_message_size;
    if (buffer_size / max_message_size != max_messages || buffer_size > static_cast<size_t>(0xFFFFFFFF))
        return std::unexpected(Error(invalid_argument, "could not create message queue '{}': bounds overflow", name));

    // FILE_FLAG_FIRST_PIPE_INSTANCE: an existing name fails ERROR_ACCESS_DENIED, like O_EXCL
    HANDLE handle = CreateNamedPipeA(pipe_path(name).c_str(),
                                     PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                                     PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                     PIPE_UNLIMITED_INSTANCES,
                                     static_cast<DWORD>(buffer_size),
                                     static_cast<DWORD>(buffer_size),
                                     0,
                                     nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_ACCESS_DENIED)
            return std::unexpected(
                Error(already_exists, "could not create message queue '{}': {}", name, os::last_error_str()));
        return std::unexpected(Error(io_error, "could not create message queue '{}': {}", name, os::last_error_str()));
    }

    _mq = reinterpret_cast<intptr_t>(handle);
    _created = true;
    _connected = false;
    _msg_size = max_message_size;
    _name = name;
    return {};
}

Result<void> MessageQueue::open(const std::string & name)
{
    auto cleared = this->clear();
    if (!cleared)
        return cleared;

    HANDLE handle = CreateFileA(pipe_path(name).c_str(),
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_FLAG_OVERLAPPED,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        const DWORD last_error = GetLastError();
        const ErrorCode code = last_error == ERROR_FILE_NOT_FOUND || last_error == ERROR_PATH_NOT_FOUND ? not_found
                                                                                                        : io_error;
        return std::unexpected(
            Error(code, "could not open message queue '{}': {}", name, os::error_str(static_cast<int>(last_error))));
    }

    DWORD read_mode = PIPE_READMODE_MESSAGE;
    if (SetNamedPipeHandleState(handle, &read_mode, nullptr, nullptr) == FALSE)
    {
        auto error = Error(io_error, "could not set pipe read mode '{}': {}", name, os::last_error_str());
        CloseHandle(handle);
        return std::unexpected(std::move(error));
    }

    DWORD out_size = 0;
    DWORD in_size = 0;
    if (GetNamedPipeInfo(handle, nullptr, &out_size, &in_size, nullptr) != FALSE)
        _msg_size = out_size > in_size ? out_size : in_size;

    _mq = reinterpret_cast<intptr_t>(handle);
    _created = false;
    _connected = true;
    _name = name;
    return {};
}

Result<void> MessageQueue::unlink(const std::string &)
{
    // named pipes are refcounted kernel objects: nothing to unlink
    return {};
}

Result<void> MessageQueue::clear()
{
    bool ret = true;
    if (_mq != -1)
    {
        HANDLE handle = reinterpret_cast<HANDLE>(_mq);
        if (_created && _connected && DisconnectNamedPipe(handle) == FALSE)
            ret = false;
        if (CloseHandle(handle) == FALSE)
            ret = false;
        _mq = -1;
        _connected = false;
        _msg_size = 0;
        _name.clear();
    }
    _created = false;
    _nonblocking = false;
    if (!ret)
        return std::unexpected(Error(io_error, "could not clear message queue"));
    return {};
}

Result<void> MessageQueue::send(ArrByteView data, unsigned priority)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    const Duration wait = _nonblocking ? Duration(0) : Duration(-1);
    auto ret = queue_send(reinterpret_cast<HANDLE>(_mq), _created, _connected, _name, wait, data, priority);
    if (!ret && _nonblocking && ret.error().code == ErrorCode::timeout)
        return std::unexpected(Error(would_block, "could not send '{}': queue is full", _name));
    return ret;
}

Result<void> MessageQueue::try_send(ArrByteView data, unsigned priority)
{
    auto ret = this->timed_send(data, Duration(0), priority);
    if (!ret && ret.error().code == ErrorCode::timeout)
        return std::unexpected(Error(would_block, "could not send '{}': queue is full", _name));
    return ret;
}

Result<void> MessageQueue::timed_send(ArrByteView data, Duration timeout, unsigned priority)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    return queue_send(reinterpret_cast<HANDLE>(_mq), _created, _connected, _name, timeout, data, priority);
}

Result<std::string> MessageQueue::receive(unsigned *priority)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    const Duration wait = _nonblocking ? Duration(0) : Duration(-1);
    auto ret = queue_receive(reinterpret_cast<HANDLE>(_mq), _created, _connected, _name, _msg_size, wait, priority);
    if (!ret && _nonblocking && ret.error().code == ErrorCode::timeout)
        return std::unexpected(Error(would_block, "could not receive '{}': queue is empty", _name));
    return ret;
}

std::optional<std::string> MessageQueue::try_receive(unsigned *priority)
{
    auto ret = this->timed_receive(Duration(0), priority);
    if (!ret)
        return std::nullopt;
    return std::move(*ret);
}

Result<std::string> MessageQueue::timed_receive(Duration timeout, unsigned *priority)
{
    if (_mq == -1)
        return std::unexpected(Error(not_initialized, "message queue is not open"));

    return queue_receive(reinterpret_cast<HANDLE>(_mq), _created, _connected, _name, _msg_size, timeout, priority);
}

Result<MessageQueue::Attributes> MessageQueue::attributes() const
{
    return std::unexpected(Error(not_supported, "no message queue attributes on windows"));
}

Result<void> MessageQueue::set_nonblocking(bool active)
{
    _nonblocking = active;
    return {};
}

} // namespace sihd::sys
