#include <sihd/zip/ZipUtils.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::zip
{

SIHD_NEW_LOGGER("sihd::zip");

std::string ZipUtils::get_error(int code)
{
    zip_error_t ziperror;
    zip_error_init_with_code(&ziperror, code);
    return zip_error_strerror(&ziperror);
}

std::string ZipUtils::get_error(zip_t *ptr)
{
    return zip_strerror(ptr);
}

}