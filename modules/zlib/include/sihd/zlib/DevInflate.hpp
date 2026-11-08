#ifndef __SIHD_ZLIB_DEVINFLATE_HPP__
#define __SIHD_ZLIB_DEVINFLATE_HPP__

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>

#include <sihd/core/Device.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/zlib/Inflater.hpp>

namespace sihd::zlib
{

// every linked channel NAME is decompressed on NAME_out
class DevInflate: public sihd::core::Device
{
    public:
        DevInflate(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevInflate();

        bool is_running() const override;

        // stream: the writes continue one stream, message: every write must be a complete stream
        bool set_mode(std::string_view mode);
        // capacity of every NAME_out, a larger decompressed write is dropped
        bool set_output_capacity(size_t capacity);

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
                std::unique_ptr<Inflater> inflater;
        };

        sihd::util::Result<std::unique_ptr<Inflater>> _new_inflater() const;

        bool _message_mode;
        size_t _output_capacity;
        std::atomic<bool> _running;
        std::mutex _mutex;
        std::map<sihd::core::Channel *, Lane> _lanes;
};

} // namespace sihd::zlib

#endif
