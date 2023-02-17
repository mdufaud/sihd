#ifndef __SIHD_CORE_CHANNELCONTAINER_HPP__
# define __SIHD_CORE_CHANNELCONTAINER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IHandler.hpp>
# include <sihd/util/str.hpp>

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

        template <typename ...T>
        std::array<Channel *, sizeof...(T)> find_channels(const T & ...args)
        {
            std::array<Channel *, sizeof...(T)> array;
            int i = 0;

            ([&]
            {
                array[i] = this->find_channel(args);
                if (array[i] == nullptr)
                    throw std::runtime_error(util::str::format("'%s'no such channel '%s'", this->full_name(), args));
                ++i;
            } (), ...);

            return array;
        }

        Channel *get_channel(const std::string & name);
        bool get_channel(const std::string & name, Channel **to_fill);

        template <typename ...T>
        std::array<Channel *, sizeof...(T)> get_channels(const T & ...args)
        {
            std::array<Channel *, sizeof...(T)> array;
            int i = 0;

            ([&]
            {
                array[i] = this->get_channel(args);
                if (array[i] == nullptr)
                    throw std::runtime_error(util::str::format("'%s' no such channel '%s'", this->full_name(), args));
                ++i;
            } (), ...);

            return array;
        }

        // store channel configuration, when links are resolved, create the channel if unlinked or get the linked one
        Channel *add_unlinked_channel(const std::string & name, sihd::util::Type type, size_t size = 1, bool check_match = true);
        Channel *add_unlinked_channel(const std::string & name, std::string_view type, size_t size = 1, bool check_match = true);

        // creates a channel and adds it as a child
        Channel *add_channel(const std::string & name, sihd::util::Type type, size_t size = 1);
        Channel *add_channel(const std::string & name, std::string_view type, size_t size = 1);

        bool observe_channel(const std::string & channel_name);
        bool observe_channel(Channel *c);
        void remove_channels_observation();

    protected:
        virtual bool on_check_link(const std::string & name, sihd::util::Named *child) override;

    private:
        struct ChannelConfiguration
        {
            sihd::util::Type type;
            size_t size;
            bool match;
        };
        std::map<std::string, ChannelConfiguration> _channels_link;
        std::list<Channel *> _observed_channels;
};

}

#endif