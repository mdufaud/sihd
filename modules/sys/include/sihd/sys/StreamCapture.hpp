#ifndef __SIHD_SYS_STREAMCAPTURE_HPP__
#define __SIHD_SYS_STREAMCAPTURE_HPP__

#include <cstdio>
#include <string>

namespace sihd::sys
{

// Redirects a C stream to a temporary file until destroyed: the capture is
// process-wide and nested captures restore in LIFO order.
class StreamCapture
{
    public:
        explicit StreamCapture(std::FILE *stream);
        ~StreamCapture();

        StreamCapture(const StreamCapture &) = delete;
        StreamCapture & operator=(const StreamCapture &) = delete;

        // flushes the stream and returns everything captured so far
        std::string str();

    private:
        std::FILE *_stream;
        int _stream_fd;
        int _saved_fd;
        std::FILE *_tmp_file;
};

} // namespace sihd::sys

#endif
