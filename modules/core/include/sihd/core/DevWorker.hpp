#ifndef __SIHD_CORE_DEVWORKER_HPP__
#define __SIHD_CORE_DEVWORKER_HPP__

#include <atomic>
#include <map>
#include <optional>

#include <sihd/core/Device.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/SafeQueue.hpp>
#include <sihd/util/Worker.hpp>

namespace sihd::core
{

// every linked channel NAME is rewritten on NAME_out by the device thread
class DevWorker: public sihd::core::Device,
                 protected sihd::util::IRunnable
{
    public:
        DevWorker(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~DevWorker();

        bool is_running() const override;

        bool set_queue_size(size_t queue_size);

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *c) override;

        bool run() override;

        bool on_init() override;
        bool on_start() override;
        bool on_stop() override;
        bool on_reset() override;

        bool bind_relays();
        void observe_relays();
        // nullptr when the channel is not relayed
        Channel *relay_out(Channel *in) const;
        const std::map<Channel *, Channel *> & relays() const;
        size_t queue_size() const;
        void count_drop();
        void publish_dropped();

    private:
        struct Item
        {
                Channel *out;
                sihd::util::ArrByte data;
        };

        size_t _queue_size;
        std::map<Channel *, Channel *> _out_of;
        std::optional<sihd::util::SafeQueue<Item>> _queue;
        sihd::util::Worker _worker;

        std::atomic<uint64_t> _dropped;
        std::atomic<uint64_t> _published_dropped;
        std::atomic<bool> _publishing;
        Channel *_channel_dropped_ptr;
};

} // namespace sihd::core

#endif
