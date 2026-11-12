#include "att.hpp"

namespace sihd::bt::att
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;

Error error_of(int status, std::string_view what)
{
    ErrorCode code = io_error;
    switch (status)
    {
        // read / write not permitted, insufficient authentication, authorization or encryption
        case 0x02:
        case 0x03:
        case 0x05:
        case 0x08:
        case 0x0f:
            code = permission_denied;
            break;

        // invalid offset, invalid attribute value length
        case 0x07:
        case 0x0d:
            code = invalid_argument;
            break;

        // request not supported
        case 0x06:
            code = not_supported;
            break;

        default:
            break;
    }
    return Error(code, "{}: gatt status 0x{:02x}", what, status);
}

} // namespace sihd::bt::att
