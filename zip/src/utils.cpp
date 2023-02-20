#include <zip.h>

#include <sihd/zip/utils.hpp>

namespace sihd::zip::utils
{

std::string get_error(int code)
{
    zip_error_t ziperror;
    zip_error_init_with_code(&ziperror, code);
    return zip_error_strerror(&ziperror);
}

std::string get_error(zip_t *ptr)
{
    return zip_strerror(ptr);
}

} // namespace sihd::zip::utils