#ifndef __SIHD_UTIL_ERROR_HPP__
#define __SIHD_UTIL_ERROR_HPP__

#include <cerrno>
#include <expected>
#include <string>
#include <system_error>
#include <utility>

#include <fmt/core.h>

// bails out of the function propagating the error: `res` must be a stored std::expected<..., Error>,
// not a call — the macro names its argument twice
#define SIHD_UNEXPECTED_RETURN(res)                                                                                    \
    if ((res).has_value() == false)                                                                                    \
        return std::unexpected(std::move(res).error());

// same bail out, rebuilding the Error with the caller context appended: the code is kept, the old message
// comes first, fmt must be a string literal — `res` is named twice and args are evaluated on error only
#define SIHD_UNEXPECTED_RETURN_CTX(res, fmt, ...)                                                                      \
    if ((res).has_value() == false)                                                                                    \
    {                                                                                                                  \
        ::sihd::util::Error __sihd_unexpected_error__ = std::move(res).error();                                        \
        return std::unexpected(::sihd::util::Error(__sihd_unexpected_error__.code,                                     \
                                                   "{}: " fmt,                                                         \
                                                   std::move(__sihd_unexpected_error__.message) __VA_OPT__(, )         \
                                                       __VA_ARGS__));                                                  \
    }

namespace sihd::util
{

// enum values are part of the api: append only, never reorder
enum class ErrorCode : int
{
    none = 0,
    unknown,
    invalid_argument,
    not_found,
    permission_denied,
    timeout,
    would_block,
    interrupted,
    out_of_memory,
    io_error,
    not_supported,
    already_exists,
    not_initialized,
    closed,
    overflow,
};

// errno has no type of its own; any intervening call may clobber it
[[nodiscard]] ErrorCode error_errno(int errno_value);

// an unresolved native error type is a compile error: each domain declares its ErrorResolver specialization
template <typename T>
struct ErrorResolver;

template <typename T>
[[nodiscard]] ErrorCode error_from(T native)
{
    return ErrorResolver<T>::code(native);
}

struct Error
{
        ErrorCode code = ErrorCode::none;
        std::string message;

        // the failure is transient: the same operation can succeed on a retry
        bool retryable() const;

        Error() = default;
        Error(ErrorCode code, std::string message);

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        Error(ErrorCode code, fmt::format_string<Args...> format, Args &&...args):
            Error(code, fmt::format(format, std::forward<Args>(args)...))
        {
        }

        template <typename Native, typename... Args>
            requires(sizeof...(Args) != 0)
        Error(Native native, fmt::format_string<Args...> format, Args &&...args):
            Error(resolve_native(native), fmt::format(format, std::forward<Args>(args)...))
        {
        }

        template <typename Native>
        Error(Native native, std::string message): Error(resolve_native(native), std::move(message))
        {
        }

        // snapshots errno once: any call in between, message() included, may clobber it
        template <typename... Args>
        static Error from_errno(fmt::format_string<Args...> format, Args &&...args)
        {
            const int errno_value = errno;
            // a failure that left errno at 0 was mis-sourced: an error must never carry the success code
            if (errno_value == 0)
                return Error(ErrorCode::unknown,
                             fmt::format(format, std::forward<Args>(args)...).append(": errno was not set"));
            return Error(error_errno(errno_value),
                         fmt::format(format, std::forward<Args>(args)...)
                             .append(": ")
                             .append(std::generic_category().message(errno_value)));
        }

    private:
        template <typename Native>
        static ErrorCode resolve_native(Native native)
        {
            static_assert(
                requires { ErrorResolver<Native>::code; },
                "no ErrorResolver specialization for this type: declare one, or pass an explicit ErrorCode");
            return error_from(native);
        }
};

template <>
struct ErrorResolver<std::errc>
{
        static ErrorCode code(std::errc ec);
};

template <>
struct ErrorResolver<std::error_code>
{
        static ErrorCode code(std::error_code ec);
};

} // namespace sihd::util

#endif
