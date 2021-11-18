#include <sihd/zip/Zip.hpp>
#include <sihd/util/Logger.hpp>
#include <zip.h>

namespace sihd::zip
{

NEW_LOGGER("sihd::zip");

Zip::Zip()
{
    int err;
    struct zip *za;
    std::string archive;
    char buf[100];

    if ((za = zip_open(archive.c_str(), 0, &err)) == NULL)
    {
        zip_error_to_str(buf, sizeof(buf), err, errno);
        LOG(error, "Can't open zip archive " << archive << ": " << buf);
    }
}

Zip::~Zip()
{
}

}