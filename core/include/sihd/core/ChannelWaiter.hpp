#ifndef __SIHD_CORE_CHANNELWAITER_HPP__
# define __SIHD_CORE_CHANNELWAITER_HPP__

# include <sihd/core/Channel.hpp>
# include <sihd/util/Waitable.hpp>

namespace sihd::core
{

class ChannelWaiter: public sihd::util::IObserver<Channel>
{
    public:
        ChannelWaiter(Channel *c = nullptr);
        virtual ~ChannelWaiter();

        bool set_channel(Channel *c);
        void clear_channel();
        bool wait_for(time_t nano, uint32_t notifications = 1);

    protected:
    
    private:
        void observable_changed(Channel *channel);

        Channel *_channel;
        sihd::util::Waitable _waitable;

};

}

#endif