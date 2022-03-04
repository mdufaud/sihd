#ifndef __SIHD_CORE_CHANNELCONTAINER_HPP__
# define __SIHD_CORE_CHANNELCONTAINER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IHandler.hpp>
# include <sihd/util/Str.hpp>
# include <sihd/core/Channel.hpp>

namespace sihd::core
{

class AChannelContainer:    public sihd::util::Node,
                            public sihd::util::Configurable,
                            public sihd::util::IHandler<Channel *>
{
    public:
        AChannelContainer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~AChannelContainer();

        Channel *find_channel(const std::string & name);
        bool find_channel(const std::string & name, Channel **to_fill);

        Channel *get_channel(const std::string & name);
        bool get_channel(const std::string & name, Channel **to_fill);

        Channel *add_unlinked_channel(const std::string & name, sihd::util::Type type, size_t size = 1);
        Channel *add_unlinked_channel(const std::string & name, const std::string & type, size_t size = 1);
        Channel *add_channel(const std::string & name, sihd::util::Type type, size_t size = 1);
        Channel *add_channel(const std::string & name, const std::string & type, size_t size = 1);

        bool observe_channel(const std::string & channel_name);
        bool observe_channel(Channel *c);
        void remove_channels_observation();

    protected:
        virtual bool _check_link(const std::string & name, sihd::util::Named *child) override;

    private:
        struct ChannelConfiguration
        {
            sihd::util::Type type;
            size_t size;
        };
        std::map<std::string, ChannelConfiguration> _channels_link;
        std::list<Channel *> _observed_channels;
};

}

#endif