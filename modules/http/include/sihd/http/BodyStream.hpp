#ifndef __SIHD_HTTP_BODYSTREAM_HPP__
#define __SIHD_HTTP_BODYSTREAM_HPP__

#include <functional>
#include <string>

#include <sihd/http/HttpRequest.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/ICloneable.hpp>

namespace sihd::http
{

class IBodyStream: public sihd::util::ICloneable<IBodyStream>
{
    public:
        virtual ~IBodyStream() = default;

        // the server clones the stream for each request and destroys the copy once the body
        // ended; returning false rejects the body and answers 500 Internal Server Error
        virtual bool on_chunk(const HttpRequest & request, sihd::util::ArrCharView chunk) = 0;
};

class BodyStreamFunc: public IBodyStream
{
    public:
        using Func = std::function<bool(const HttpRequest & request, sihd::util::ArrCharView chunk)>;

        BodyStreamFunc() = default;
        explicit BodyStreamFunc(Func func);

        void set_func(Func func);

        virtual IBodyStream *clone() const override;
        virtual bool on_chunk(const HttpRequest & request, sihd::util::ArrCharView chunk) override;

    private:
        Func _func;
};

class BodyStreamFile: public IBodyStream
{
    public:
        using PathProvider = std::function<std::string(const HttpRequest & request)>;

        BodyStreamFile() = default;
        explicit BodyStreamFile(std::string path);
        explicit BodyStreamFile(PathProvider path_provider);

        void set_path(std::string path);
        void set_path_provider(PathProvider path_provider);

        virtual IBodyStream *clone() const override;
        virtual bool on_chunk(const HttpRequest & request, sihd::util::ArrCharView chunk) override;

    private:
        BodyStreamFile(const BodyStreamFile & other);

        PathProvider _path_provider;
        sihd::sys::File _file;
};

} // namespace sihd::http

#endif
