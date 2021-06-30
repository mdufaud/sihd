#ifndef __SIHD_UTIL_BUFFER_HPP__
# define __SIHD_UTIL_BUFFER_HPP__

# include <cstdint>
# include <string.h>
# include <stdexcept>
# include <memory>
# include <vector>
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

        virtual uint8_t *data() = 0;
        virtual size_t  data_size() = 0;
        virtual size_t  size() = 0;
        virtual size_t  capacity() = 0;
        virtual bool    assign(uint8_t *buf, size_t size) = 0;
        virtual bool    from(IBuffer & buf) = 0;

        virtual Datatypes    data_type() = 0;
        virtual std::string   data_type_to_string() = 0;

        virtual Endian::Endianness  get_endianness() = 0;
};

template <typename T>
class Buffer:   virtual public IBuffer,
                virtual public ICloneable<Buffer<T>>
{
    public:
        Buffer()
        {
            endianness = Endian::get_endian();
            _buffer.resize(1);
        }

        Buffer(size_t elements)
        {
            endianness = Endian::get_endian();
            _buffer.resize(elements);
        }

        Buffer(size_t elements, Endian::Endianness endian)
        {
            endianness = endian;
            _buffer.resize(elements);
        }

        virtual ~Buffer()
        {
        }

        Endian::Endianness  endianness;

        // IBuffer

        virtual uint8_t *data() { return (uint8_t *)_buffer.data(); }
        virtual size_t  data_size() { return sizeof(T); }
        virtual size_t  size() { return _buffer.size(); }
        virtual size_t  capacity() { return _buffer.capacity(); }
        virtual bool    assign(uint8_t *buf, size_t size)
        {
            if (size % this->data_size() != 0)
                return false;
            _buffer.clear();
            _buffer.resize(size / this->data_size());
            memcpy(_buffer.data(), buf, size);
            return true;
        };
        virtual bool    from(IBuffer & obj)
        {
            if (obj.data_type() != this->data_type())
                return false;
            this->endianness = obj.get_endianness();
            _buffer = std::vector<T>(obj.capacity());
            memcpy(this->data(), obj.data(), obj.size() * obj.data_size());
            return _buffer.size() == obj.size();
        }

        virtual Endian::Endianness  get_endianness() { return this->endianness; }
        virtual Datatypes    data_type() { return Datatype::type_to_datatype<T>(); }
        virtual std::string   data_type_to_string() { return Datatype::datatype_to_string(this->data_type()); }

        // ICloneable

        virtual std::unique_ptr<Buffer<T>>    clone()
        {
            std::unique_ptr<Buffer<T>> ret = std::make_unique<Buffer<T>>();
            ret.get()->from(*this);
            return ret;
        }

        // Class methods

        inline T    operator[](size_t idx) const
        {
            if (idx >= _buffer.size())
                throw std::out_of_range("Buffer read[] out of range");
            return _buffer[idx];
        }

        inline T &  operator[](size_t idx)
        {
            if (idx >= _buffer.size())
                throw std::out_of_range("Buffer assign[] out of range");
            return _buffer[idx];
        }

        inline T &  at(size_t idx)
        {
            if (idx >= _buffer.size())
                throw std::out_of_range("Buffer assign[] out of range");
            return _buffer[idx];
        }

        std::vector<T>  *buffer()
        {
            return &_buffer;
        }

    private:
        std::vector<T>  _buffer;
};

typedef Buffer<bool>       Bool;
typedef Buffer<int8_t>     Byte;
typedef Buffer<uint8_t>    UByte;
typedef Buffer<int16_t>    Short;
typedef Buffer<uint16_t>   UShort;
typedef Buffer<int32_t>    Int;
typedef Buffer<uint32_t>   UInt;
typedef Buffer<int64_t>    Long;
typedef Buffer<uint64_t>   ULong;
typedef Buffer<float>      Float;
typedef Buffer<double>     Double;
// TODO
//typedef Buffer<std::string>    String;

}

#endif 