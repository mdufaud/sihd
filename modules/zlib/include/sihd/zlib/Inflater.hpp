#ifndef __SIHD_ZLIB_INFLATER_HPP__
#define __SIHD_ZLIB_INFLATER_HPP__

#include <memory>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::zlib
{

// zlib and gzip input are told apart by their header
class Inflater
{
    public:
        explicit Inflater(size_t max_output_size);
        ~Inflater();

        Inflater(const Inflater &) = delete;
        Inflater & operator=(const Inflater &) = delete;

        // continues the stream, a stream end restarts on the next header
        sihd::util::Result<sihd::util::ArrByte> inflate_stream(sihd::util::ArrByteView in);
        sihd::util::Result<sihd::util::ArrByte> inflate_message(sihd::util::ArrByteView in);

    private:
        struct Impl;

        sihd::util::Result<sihd::util::ArrByte> _inflate(sihd::util::ArrByteView in, bool single_message);

        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::zlib

#endif
