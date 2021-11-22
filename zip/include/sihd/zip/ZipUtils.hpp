#ifndef __SIHD_ZIP_ZIPUTILS_HPP__
# define __SIHD_ZIP_ZIPUTILS_HPP__

# include <zip.h>
# include <string>

namespace sihd::zip
{

class ZipUtils
{
    public:
        static std::string get_error(int code);
        static std::string get_error(zip_t *ptr);

    protected:
    
    private:
        ZipUtils() {};
        ~ZipUtils() {};
};

}

#endif