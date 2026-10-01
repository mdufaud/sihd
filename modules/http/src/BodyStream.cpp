#include <stdexcept>

#include <sihd/http/BodyStream.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::http
{

SIHD_LOGGER;

BodyStreamFunc::BodyStreamFunc(Func func)
{
    this->set_func(std::move(func));
}

void BodyStreamFunc::set_func(Func func)
{
    _func = std::move(func);
}

IBodyStream *BodyStreamFunc::clone() const
{
    return new BodyStreamFunc(*this);
}

bool BodyStreamFunc::on_chunk(const HttpRequest & request, sihd::util::ArrCharView chunk)
{
    if (_func == nullptr)
        throw std::logic_error("body stream func is not set");
    return _func(request, chunk);
}

BodyStreamFile::BodyStreamFile(std::string path)
{
    this->set_path(std::move(path));
}

BodyStreamFile::BodyStreamFile(PathProvider path_provider)
{
    this->set_path_provider(std::move(path_provider));
}

void BodyStreamFile::set_path(std::string path)
{
    this->set_path_provider([path = std::move(path)](const HttpRequest &) { return path; });
}

void BodyStreamFile::set_path_provider(PathProvider path_provider)
{
    _path_provider = std::move(path_provider);
}

BodyStreamFile::BodyStreamFile(const BodyStreamFile & other): _path_provider(other._path_provider) {}

IBodyStream *BodyStreamFile::clone() const
{
    return new BodyStreamFile(*this);
}

bool BodyStreamFile::on_chunk(const HttpRequest & request, sihd::util::ArrCharView chunk)
{
    if (_path_provider == nullptr)
        throw std::logic_error("body stream path is not set");
    if (_file.is_open() == false)
    {
        auto opened = _file.open(_path_provider(request), "wb");
        if (!opened)
        {
            SIHD_LOG(error, "BodyStream: cannot open '{}' for writing", _path_provider(request));
            return false;
        }
    }
    const auto wrote = _file.write(chunk);
    return wrote && *wrote == chunk.size();
}

} // namespace sihd::http
