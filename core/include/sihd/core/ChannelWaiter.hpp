#ifndef __SIHD_CORE_CHANNELWAITER_HPP__
# define __SIHD_CORE_CHANNELWAITER_HPP__

# include <sihd/util/Waitable.hpp>

# include <sihd/core/Channel.hpp>
# include <sihd/core/ACoreObject.hpp>

namespace sihd::core
{

class ChannelWaiter: public ACoreObject
{
    public:
        ChannelWaiter(const std::string & name, sihd::util::Node *parent = nullptr);
        ChannelWaiter(Channel *c = nullptr);
        virtual ~ChannelWaiter();

        bool set_channel(Channel *c);
        void clear_channel();
        bool wait_for(time_t nano, uint32_t notifications = 1);

        int notifications() const { return _count.load(); }

    protected:
        bool do_stop() override;

    private:
        void handle(Channel *channel) override;

        Channel *_channel;
        sihd::util::Waitable _waitable;
        std::atomic<int> _count;

};

}

#endif