#ifndef __SIHD_UTIL_IBUFFER_HPP__
# define __SIHD_UTIL_IBUFFER_HPP__

# include <cstdint>
# include <string.h>
# include <stdexcept>
# include <memory>
# include <sihd/util/Endian.hpp>
# include <sihd/util/ICloneable.hpp>
# include <sihd/util/Datatype.hpp>
# include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

class IBuffer
{
    public:
        virtual ~IBuffer() {};

        virtual uint8_t *buf() = 0;
        virtual size_t  data_size() = 0;
        virtual size_t  size() = 0;
        virtual size_t  capacity() = 0;
        virtual void    assign(uint8_t *buf, size_t size) = 0;
        virtual bool    from(IBuffer & buf) = 0;

        virtual Datatypes    data_type() = 0;
        virtual std::string   data_type_to_string() = 0;

        virtual Endian::Endianness  get_endianness() = 0;
};

}

#endif 