#include <cerrno>

#include <sihd/util/Error.hpp>

#if defined(_WIN32)
# include <winsock2.h>
#endif

namespace sihd::util
{

namespace
{

ErrorCode errno_to_error_code(int errno_value)
{
    switch (errno_value)
    {
        case 0:
            return ErrorCode::none;
        case EINVAL:
        case ENOTSOCK:
            return ErrorCode::invalid_argument;
        case ENOENT:
        case ENOTDIR:
        case ELOOP:
        case ENODEV:
        case ESRCH:
            return ErrorCode::not_found;
        case EACCES:
        case EPERM:
        case EROFS:
            return ErrorCode::permission_denied;
        case ETIMEDOUT:
#if defined(ETIME)
        case ETIME:
#endif
            return ErrorCode::timeout;
        case EAGAIN:
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return ErrorCode::would_block;
        case EINPROGRESS:
            return ErrorCode::would_block;
        case EINTR:
            return ErrorCode::interrupted;
        case ENOMEM:
        case ENOBUFS:
            return ErrorCode::out_of_memory;
        case EIO:
        case EILSEQ:
        case EBADMSG:
        case EPROTO:
            return ErrorCode::io_error;
        case ENOSYS:
#if defined(ENOTSUP)
        case ENOTSUP:
#endif
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP // mingw: own errno, posix aliases ENOTSUP
        case EOPNOTSUPP:
#endif
            return ErrorCode::not_supported;
        case EEXIST:
        case EADDRINUSE:
            return ErrorCode::already_exists;
        case E2BIG:
        case ERANGE:
#if defined(EFBIG)
        case EFBIG:
#endif
#if defined(ENAMETOOLONG)
        case ENAMETOOLONG:
#endif
            return ErrorCode::overflow;
        case EBADF:
        case EPIPE:
        case ECONNABORTED:
        case ECONNRESET:
        case ECONNREFUSED:
        case ENOTCONN:
        case ENETDOWN:
        case ENETRESET:
        case ENETUNREACH:
        case EHOSTUNREACH:
#if defined(EHOSTDOWN)
        case EHOSTDOWN:
#endif
#if defined(ESHUTDOWN)
        case ESHUTDOWN:
#endif
            return ErrorCode::closed;
        case EALREADY:
            return ErrorCode::would_block;
        case EISCONN:
            return ErrorCode::io_error;
        case EMSGSIZE:
        case EOVERFLOW:
            return ErrorCode::overflow;
        case EDESTADDRREQ:
        case EADDRNOTAVAIL:
            return ErrorCode::invalid_argument;
        case EMFILE:
        case ENFILE:
            return ErrorCode::out_of_memory;
        case ENOPROTOOPT:
        case EAFNOSUPPORT:
        case EPROTONOSUPPORT:
#if defined(ESOCKTNOSUPPORT)
        case ESOCKTNOSUPPORT:
#endif
#if defined(EPFNOSUPPORT)
        case EPFNOSUPPORT:
#endif
            return ErrorCode::not_supported;
        default:
            break;
    }
#if defined(_WIN32)
    switch (errno_value)
    {
        case WSAEWOULDBLOCK:
        case WSAEINPROGRESS:
        case WSAEALREADY:
            return ErrorCode::would_block;
        case WSAETIMEDOUT:
            return ErrorCode::timeout;
        case WSAEINTR:
            return ErrorCode::interrupted;
        case WSAEBADF:
        case WSAECONNRESET:
        case WSAECONNABORTED:
        case WSAECONNREFUSED:
        case WSAENOTCONN:
        case WSAENETDOWN:
        case WSAENETRESET:
        case WSAENETUNREACH:
        case WSAEHOSTUNREACH:
        case WSAEHOSTDOWN:
        case WSAESHUTDOWN:
        case WSAEDISCON:
            return ErrorCode::closed;
        case WSAEISCONN:
            return ErrorCode::io_error;
        case WSAEACCES:
            return ErrorCode::permission_denied;
        case WSAEINVAL:
        case WSAENOTSOCK:
        case WSAEDESTADDRREQ:
        case WSAEADDRNOTAVAIL:
            return ErrorCode::invalid_argument;
        case WSAEADDRINUSE:
            return ErrorCode::already_exists;
        case WSAEMSGSIZE:
            return ErrorCode::overflow;
        case WSAEMFILE:
        case WSAENOBUFS:
            return ErrorCode::out_of_memory;
        case WSAENOPROTOOPT:
        case WSAEOPNOTSUPP:
        case WSAEAFNOSUPPORT:
        case WSAEPROTONOSUPPORT:
        case WSAESOCKTNOSUPPORT:
        case WSAEPFNOSUPPORT:
            return ErrorCode::not_supported;
        case WSAELOOP:
            return ErrorCode::not_found;
        case WSAENAMETOOLONG:
            return ErrorCode::overflow;

        case WSANOTINITIALISED:
            return ErrorCode::not_initialized;
    }
#endif
    return ErrorCode::unknown;
}

} // namespace

Error::Error(ErrorCode code, std::string message): code(code), message(std::move(message)) {}

// would-block, wait timeout, signal interruption: the classic transient family
bool Error::retryable() const
{
    return code == ErrorCode::would_block || code == ErrorCode::timeout || code == ErrorCode::interrupted;
}

ErrorCode error_errno(int errno_value)
{
    return errno_to_error_code(errno_value);
}

ErrorCode ErrorResolver<std::errc>::code(std::errc ec)
{
    // std::errc values are the errno ones: one table decides both
    return errno_to_error_code(static_cast<int>(ec));
}

ErrorCode ErrorResolver<std::error_code>::code(std::error_code ec)
{
    // only the generic category carries errno values
    return ec.category() == std::generic_category() ? error_errno(ec.value()) : ErrorCode::unknown;
}

} // namespace sihd::util
