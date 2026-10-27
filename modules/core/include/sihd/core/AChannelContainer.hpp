#ifndef __SIHD_CORE_ACHANNELCONTAINER_HPP__
#define __SIHD_CORE_ACHANNELCONTAINER_HPP__

#include <map>
#include <string>

#include <sihd/core/Channel.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/str.hpp>

namespace sihd::core
{

class AChannelContainer: public sihd::util::Node,
                         public sihd::util::Configurable,
                         public sihd::util::IHandler<Channel *>
{
    public:
        AChannelContainer(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~AChannelContainer();

        sihd::util::Result<Channel *> find_channel(const std::string & name);

        sihd::util::Result<Channel *> get_channel(const std::string & name);

        // linked channels except declared ports, once links are resolved: an error if a link is not a channel
        sihd::util::Result<std::map<std::string, Channel *>> linked_channels();

        // store channel configuration, created if unlinked or fetched from the link once links are resolved
        Channel *add_unlinked_channel(const std::string & name,
                                      sihd::util::Type type,
                                      size_t size = 1,
                                      bool check_match = true);
        Channel *add_unlinked_channel(const std::string & name,
                                      std::string_view type_str,
                                      size_t size = 1,
                                      bool check_match = true);

        Channel *add_channel(const std::string & name, sihd::util::Type type, size_t size = 1);
        Channel *add_channel(const std::string & name, std::string_view type_str, size_t size = 1);

        // the resizable flag is always verified at link time, check_match additionally gates type and capacity
        Channel *add_unlinked_channel_resizable(const std::string & name,
                                                sihd::util::Type type,
                                                size_t size,
                                                size_t capacity,
                                                bool check_match = true);
        Channel *add_unlinked_channel_resizable(const std::string & name,
                                                std::string_view type_str,
                                                size_t size,
                                                size_t capacity,
                                                bool check_match = true);

        Channel *add_channel_resizable(const std::string & name, sihd::util::Type type, size_t size, size_t capacity);
        Channel *
            add_channel_resizable(const std::string & name, std::string_view type_str, size_t size, size_t capacity);

        sihd::util::Result<void> observe_channel(const std::string & channel_name);
        bool observe_channel(Channel *c);
        void remove_channels_observation();

    protected:
        virtual bool on_check_link(const std::string & name, sihd::util::Named *child) override;

    private:
        struct ChannelConfiguration
        {
                sihd::util::Type type;
                size_t size;
                size_t capacity;
                bool match;
                bool resizable;
        };
        std::map<std::string, ChannelConfiguration> _channels_link;
        std::list<Channel *> _observed_channels;
};

} // namespace sihd::core

#endif
