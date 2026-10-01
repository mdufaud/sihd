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
