#ifndef __SIHD_ZLIB_DEVDEFLATE_HPP__
#define __SIHD_ZLIB_DEVDEFLATE_HPP__

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>

#include <sihd/core/Device.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/zlib/Deflater.hpp>

namespace sihd::zlib
{

// every linked channel NAME is compressed on NAME_out
class DevDeflate: public sihd::core::Device
{
    public:
        DevDeflate(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevDeflate();

        bool is_running() const override;

        // stream: the writes form one stream, message: every write is a complete stream
        bool set_mode(std::string_view mode);
        bool set_level(int level);
        bool set_format(std::string_view format);

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *c) override;

        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;
        sihd::util::Result<void> on_links_resolved() override;

    private:
        struct Lane
        {
                sihd::core::Channel *out = nullptr;
                std::unique_ptr<Deflater> deflater;
        };

        sihd::util::Result<std::unique_ptr<Deflater>> _new_deflater() const;

        bool _message_mode;
        int _level;
        Deflater::Format _format;
        std::atomic<bool> _running;
        std::mutex _mutex;
        std::map<sihd::core::Channel *, Lane> _lanes;
};

} // namespace sihd::zlib

#endif
